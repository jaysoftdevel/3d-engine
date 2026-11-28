"""Stream build commands and summarize the outputs for one CMake configuration."""

import json
import locale
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time

from project_guard import cache_values, no_symlinks


def run_stage(command, log, env=None):
    no_symlinks(log)
    display = subprocess.list2cmdline(command) if os.name == 'nt' else shlex.join(command)
    print(f'Running: {display}', flush=True)
    with log.open('w', encoding='utf-8', buffering=1) as handle, subprocess.Popen(
        command, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        encoding=locale.getpreferredencoding(False), errors='replace',
    ) as process:
        for line in process.stdout:
            handle.write(line)
            print(line, end='', flush=True)
        status = process.wait()
    if status:
        print(f'Failure log: {log}', file=sys.stderr)
        raise subprocess.CalledProcessError(status, command)
    print(f'Log: {log}', flush=True)


def library_file(name):
    return bool(re.search(r'(?:\.a|\.lib|\.dll|\.so(?:\.\d+)*|\.dylib)$', name, re.IGNORECASE))


def arguments(fragment):
    if os.name == 'nt':
        # Preserve drive paths and quotes embedded in /LIBPATH: arguments.
        return [arg.replace('"', '') for arg in re.findall(r'(?:[^"\s]+|"[^"]*")+', fragment)]
    return shlex.split(fragment)


def msvc_link_files(target, build, configuration):
    directory = build / target['paths']['build'] / (target['name'] + '.dir') / configuration
    paths = {}
    for log in directory.glob('*.tlog/link.read.*.tlog'):
        payload = log.read_bytes()
        encoding = 'utf-16' if payload.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8'
        for line in payload.decode(encoding, errors='replace').splitlines():
            path = Path(line)
            if not line.startswith('^') and library_file(path.name) and path.is_absolute() and path.is_file():
                paths[path.name.casefold()] = path
    return paths


def link_inputs(target, build, implicit, env, traced):
    link = target.get('link')
    if not link:
        return []
    toolchain = implicit.get(link['language'], {})
    working = build / target['paths']['build']
    directories = [build, working]
    names = []
    for part in link.get('commandFragments', []):
        role = part['role']
        if role == 'libraryPath':
            for arg in arguments(part['fragment']):
                value = arg.removeprefix('-L') if arg.startswith('-L') else re.sub(r'^/LIBPATH:', '', arg, flags=re.I)
                if value:
                    directories.extend((build / value, working / value))
        elif role == 'libraries':
            names.extend(arg for arg in arguments(part['fragment']) if arg.startswith('-l') or library_file(arg))
    directories.extend(Path(path) for path in toolchain.get('linkDirectories', []))
    directories.extend(Path(path) for path in env.get('LIB', '').split(os.pathsep) if path)
    names.extend(name if library_file(name) else '-l' + name for name in toolchain.get('linkLibraries', []))
    known_names = {Path(name).name.casefold() for name in names}
    names.extend(str(path) for name, path in traced.items() if name not in known_names)

    for name in dict.fromkeys(names):
        if name.startswith('-l:'):
            candidates = [name[3:]]
        elif name.startswith('-l'):
            stem = 'lib' + name[2:]
            candidates = [stem + suffix for suffix in ('.so', '.dll.a', '.a')]
        else:
            candidates = [name]
        path = traced.get(Path(name).name.casefold())
        if path is None:
            path = next((directory / filename for directory in directories for filename in candidates
                         if (directory / filename).is_file()), None)
        yield name, path


def build_targets(build, configuration):
    reply = build / '.cmake/api/v1/reply'
    # A successful configure writes a new index and drops the previous index.
    index = json.loads(max(reply.glob('index-*.json'), key=lambda path: path.name).read_text(encoding='utf-8'))
    replies = index['reply']

    def model(kind):
        return json.loads((reply / replies[kind]['jsonFile']).read_text(encoding='utf-8'))

    configs = model('codemodel-v2')['configurations']
    selected = next(config for config in configs if config['name'] == configuration)
    refs = {entry['id']: entry for entry in selected['targets']}
    roots = [entry['id'] for entry in refs.values() if selected['directories'][entry['directoryIndex']]['build'] == '.']
    targets = {}
    # Top-level targets and their dependencies exclude unrelated SDK targets in the game build.
    while roots:
        target_id = roots.pop()
        if target_id not in refs or target_id in targets:
            continue
        target = json.loads((reply / refs[target_id]['jsonFile']).read_text(encoding='utf-8'))
        targets[target_id] = target
        roots.extend(dep['id'] for dep in target.get('dependencies', []))
    implicit = {tool['language']: tool['compiler'].get('implicit', {}) for tool in model('toolchains-v1')['toolchains']}
    return index['cmake']['generator']['name'], targets.values(), implicit


def cmake_value(value):
    # Bracket strings pass paths verbatim, including spaces, dollar signs and backslashes.
    separator = ''
    while f']{separator}]' in value:
        separator += '='
    return f'[{separator}[{value}]{separator}]'


def runtime_libraries(cmake, build, executables, shared, env):
    if not executables and not shared:
        return []
    cache = cache_values(build)
    pe = any(path.suffix.lower() in ('.exe', '.dll') for path in (*executables, *shared))
    tool = cache.get('CMAKE_OBJDUMP', '')
    if pe and os.name == 'nt':
        dumpbin = Path(cache.get('CMAKE_DUMPBIN') or Path(cache.get('CMAKE_LINKER', 'link.exe')).with_name('dumpbin.exe'))
        tool = str(dumpbin) if dumpbin.is_file() else shutil.which('dumpbin', path=env.get('PATH')) or tool
    backend = 'dumpbin' if tool and Path(tool).stem.lower() == 'dumpbin' else 'objdump'
    search_dirs = sorted({path.parent for path in shared})
    if pe:
        search_dirs.extend(Path(path) for path in env.get('PATH', '').split(os.pathsep) if path and Path(path).is_dir())

    def setting(name, values):
        return f"set({name} {' '.join(cmake_value(value.as_posix() if isinstance(value, Path) else value) for value in values)})\n"

    # Ask CMake to inspect the binary headers. The produced programs are never launched.
    with tempfile.TemporaryDirectory(prefix='jsd-runtime-') as temp:
        script = Path(temp) / 'dependencies.cmake'
        resolve = Path(__file__).with_name('runtime_dependencies.cmake')
        script.write_text(
            setting('CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM', ['windows+pe' if pe else 'linux+elf']) +
            setting('CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL', [backend]) +
            (setting('CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND', [tool]) if tool and not tool.endswith('-NOTFOUND') else '') +
            setting('executables', executables) + setting('shared', shared) +
            setting('search_dirs', list(dict.fromkeys(search_dirs))) +
            f'include({cmake_value(resolve.as_posix())})\n', encoding='utf-8')
        result = subprocess.run([cmake, '-P', str(script)], env=env, capture_output=True,
                                text=True, encoding='utf-8', errors='replace', check=True)
    return sorted(set(result.stdout.splitlines()), key=str.casefold)


def size_text(path):
    size = path.stat().st_size
    for unit, factor in [('GiB', 1024 ** 3), ('MiB', 1024 ** 2), ('KiB', 1024)]:
        if size >= factor:
            return f'{size:,} bytes ({size / factor:.2f} {unit})'
    return f'{size:,} bytes'


def duration(seconds):
    minutes, seconds = divmod(seconds, 60)
    return f'{int(minutes)}m {seconds:05.2f}s' if minutes else f'{seconds:.2f}s'


class BuildReport:
    def __init__(self, cmake, build, configuration, tag, env=None):
        self.cmake = cmake
        self.build = Path(build).absolute()
        self.configuration = configuration
        self.tag = tag
        self.env = os.environ if env is None else env
        self.mode = configuration.lower()
        self.path = self.build / f'build-summary-{self.mode}.txt'
        self.phases = {}

    def __enter__(self):
        no_symlinks(self.path)
        no_symlinks(self.build / '.cmake/api/v1/query').mkdir(parents=True, exist_ok=True)
        for kind in ('codemodel-v2', 'toolchains-v1'):
            no_symlinks(self.build / '.cmake/api/v1/query' / kind).touch()
        print(f'Build tag: {self.tag} ({self.configuration})', flush=True)
        return self

    def run(self, phase, command):
        label = phase.lower()
        log = self.build / ('configure.log' if label == 'configure' else f'{label}-{self.mode}.log')
        start = time.perf_counter()
        status = 'failed'
        try:
            run_stage(command, log, self.env)
            status = 'ok'
        finally:
            self.phases[phase] = (time.perf_counter() - start, status)

    def file_line(self, path, alias=None):
        name = path.relative_to(self.build) if path.is_relative_to(self.build) else path
        if alias is not None and str(name) != alias:
            name = f'{alias} -> {name}'
        return f'{name}  |  {size_text(path)}'

    def outputs(self):
        generator, targets, implicit = build_targets(self.build, self.configuration)
        lines = [f'Generator: {generator}', '', 'Build outputs (selected configuration)']
        executables, shared = set(), set()
        products, links = [], {}
        for target in sorted(targets, key=lambda item: item['name']):
            kind = target['type']
            if kind not in ('EXECUTABLE', 'SHARED_LIBRARY', 'MODULE_LIBRARY', 'STATIC_LIBRARY'):
                continue
            for entry in target.get('artifacts', []):
                declared = self.build / entry['path']
                if not declared.is_file():
                    continue
                path = declared.resolve()
                if kind == 'EXECUTABLE' and path.suffix.lower() in ('', '.exe'):
                    executables.add(path)
                elif not library_file(path.name):
                    continue
                elif kind in ('SHARED_LIBRARY', 'MODULE_LIBRARY') and (
                    path.suffix.lower() in ('.dll', '.dylib') or '.so' in path.name
                ):
                    shared.add(path)
                products.append(f"  {target['name']}  |  {self.file_line(path)}")
            traced = msvc_link_files(target, self.build, self.configuration) if generator.startswith('Visual Studio') else {}
            for name, path in link_inputs(target, self.build, implicit, self.env, traced):
                key = (name, path.resolve() if path else None)
                links.setdefault(key, []).append(target['name'])
        lines.extend(dict.fromkeys(products or ['  No compiled artifacts found.']))
        lines.extend(['', 'Linker inputs (with consuming targets)'])
        for (name, path), users in sorted(links.items(), key=lambda item: (item[0][0].casefold(), str(item[0][1]))):
            detail = self.file_line(path, alias=name) if path else f'{name}  |  size unavailable'
            lines.append(f'  {detail}  |  {", ".join(users)}')
        if not links:
            lines.append('  None recorded.')
        lines.extend(['', 'Runtime libraries (including transitive dependencies)'])
        try:
            dependencies = runtime_libraries(self.cmake, self.build, sorted(executables), sorted(shared), self.env)
            for item in dependencies:
                state, _, value = item.partition('|')
                if state == 'file':
                    path = Path(value).resolve()
                    lines.append(f'  {self.file_line(path)}')
                else:
                    lines.append(f'  {value}  |  {state}')
            if not dependencies:
                lines.append('  None found.')
        except (OSError, subprocess.CalledProcessError) as error:
            detail = getattr(error, 'stderr', None) or str(error)
            lines.append('  Scan unavailable: ' + ' '.join(detail.split()))
        manifest = self.build / 'install_manifest.txt'
        if self.phases.get('Install', (0, ''))[1] == 'ok' and manifest.is_file():
            installed = {Path(name).resolve() for name in manifest.read_text(encoding='utf-8').splitlines()
                         if library_file(name) and Path(name).is_file()}
            lines.extend(['', 'Installed libraries', *(f'  {self.file_line(path)}' for path in sorted(installed))])
        return lines

    def __exit__(self, exc_type, *_):
        lines = ['Build summary', f'Tag: {self.tag}', f'Configuration: {self.configuration}',
                 f'Status: {"failed" if exc_type else "success"}', '',
                 'Duration by stage']
        lines.extend(f'  {name:<10} {duration(seconds):>10}  {status}'
                     for name, (seconds, status) in self.phases.items())
        lines.append(f'  {"Total":<10} {duration(sum(seconds for seconds, _ in self.phases.values())):>10}')
        if self.phases.get('Build', (0, ''))[1] == 'ok':
            lines.append('')
            try:
                lines.extend(self.outputs())
            except (KeyError, OSError, ValueError, StopIteration) as error:
                lines.append(f'CMake output metadata unavailable: {error}')
        summary = '\n'.join(lines) + '\n'
        self.path.write_text(summary, encoding='utf-8')
        print(f'\n{summary}Summary file: {self.path}', flush=True)
