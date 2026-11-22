#!/usr/bin/env python3
"""Wrong-target/version and no-write regression tests; no graphics hardware needed."""
import contextlib
import io
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import apply_update as update
import build_project as build
import project_guard as guard


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    path.chmod(0o644)


def fixture(root, pid, ver, marked=True):
    meta = dict(format='jsd-project', schema=1, project_id=pid, cmake_project=guard.PROJECTS[pid], version=ver, api_version=1, kind='source')
    write(root / 'CMakeLists.txt', f'project({guard.PROJECTS[pid]} VERSION {ver} LANGUAGES C CXX)\n')
    if pid == 'jsd.engine':
        for rel in ('include/jsd/engine.hpp', 'include/frontier/renderer.hpp'):
            write(root / rel, f'inline constexpr const char *Version = "{ver}";\n')
        write(root / 'src/runtime/runtime.cpp', f'const char *jsd_game_engine_version() {{ return "{ver}"; }}\n')
    else:
        meta['requires'] = {'engine': {'min_version': '0.5.3', 'max_version_exclusive': '0.6.0', 'api_version': 1}}
    if marked:
        write(root / 'JSD_PROJECT.json', json.dumps(meta))
    write(root / 'build.sh', 'base build\n')
    return meta


def tree(root):
    return {p.relative_to(root).as_posix(): (p.read_bytes(), p.stat().st_mode & 0o777) for p in root.rglob('*') if p.is_file() and not p.is_symlink()}


class Guards(unittest.TestCase):
    def setUp(self):
        output = contextlib.redirect_stdout(io.StringIO())
        output.__enter__();self.addCleanup(output.__exit__, None, None, None)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.engine, self.editor = self.root / 'engine', self.root / 'editor'
        fixture(self.engine, 'jsd.engine', '0.5.2', False)
        fixture(self.editor, 'jsd.editor', '1.1.0', False)
        self.package = self.root / 'update'
        self.package.mkdir()
        components = []
        for pid, base, ver in [('jsd.engine', self.engine, '0.5.3'), ('jsd.editor', self.editor, '1.1.1')]:
            target = self.package / 'files' / pid
            fixture(target, pid, ver)
            write(target / 'build.sh', 'guarded build\n')
            files = []
            for rel, (content, mode) in tree(target).items():
                old = base / rel
                files.append(dict(path=rel, sha256=guard.digest(target / rel), before_sha256=guard.digest(old) if old.exists() else None, mode=mode))
            anchors = ['CMakeLists.txt']
            if pid == 'jsd.engine':
                anchors += ['include/jsd/engine.hpp', 'include/frontier/renderer.hpp', 'src/runtime/runtime.cpp']
            components.append(dict(project_id=pid, from_versions=['0.5.2' if pid == 'jsd.engine' else '1.1.0'], legacy_versions=['0.5.2' if pid == 'jsd.engine' else '1.1.0'], target_version=ver, base_fingerprints={rel: guard.digest(base / rel) for rel in anchors}, files=files))
        self.data = dict(format='jsd-update', schema=2, package_id='guard-test', components=components)
        self.save_manifest()

    def save_manifest(self):
        write(self.package / 'manifest.json', json.dumps(self.data))

    def rejected(self, targets):
        before = {pid: tree(root) for pid, root in targets.items()}
        with self.assertRaises((guard.GuardError, OSError)):
            update.apply(self.package, targets)
        self.assertEqual(before, {pid: tree(root) for pid, root in targets.items()})

    def updated(self):
        update.apply(self.package, {'jsd.engine': self.engine, 'jsd.editor': self.editor})

    def test_engine_only_without_editor(self):
        shutil.rmtree(self.editor)
        update.apply(self.package, {'jsd.engine': self.engine})
        self.assertEqual(guard.source(self.engine, 'jsd.engine')['version'], '0.5.3')
        self.assertFalse(self.editor.exists())

    def test_editor_only_preserves_engine(self):
        before = tree(self.engine)
        update.apply(self.package, {'jsd.editor': self.editor})
        self.assertEqual(before, tree(self.engine))

    def test_combined_preflight_no_writes(self):
        write(self.editor / 'build.sh', 'local edit')
        self.rejected({'jsd.engine': self.engine, 'jsd.editor': self.editor})
        self.assertFalse((self.engine / '.jsd-operation.lock').exists())

    def test_swapped_targets(self):
        self.rejected({'jsd.engine': self.editor, 'jsd.editor': self.engine})

    def test_unexpected_versions(self):
        for ver in ('0.5.1', '0.5.12', '0.6.0'):
            fixture(self.engine, 'jsd.engine', ver, False)
            self.rejected({'jsd.engine': self.engine})

    def test_local_edit_rejected(self):
        write(self.engine / 'build.sh', 'local edit')
        self.rejected({'jsd.engine': self.engine})

    def test_partial_base_rejected(self):
        shutil.copy2(self.package / 'files/jsd.engine/build.sh', self.engine / 'build.sh')
        self.rejected({'jsd.engine': self.engine})

    def test_same_version_partial_rejected(self):
        fixture(self.engine, 'jsd.engine', '0.5.3')
        self.rejected({'jsd.engine': self.engine})

    def test_fully_applied_is_noop(self):
        self.updated()
        before = tree(self.engine), tree(self.editor)
        self.updated()
        self.assertEqual(before, (tree(self.engine), tree(self.editor)))

    def test_target_version_modified_rejected(self):
        self.updated()
        write(self.engine / 'build.sh', 'edit after update')
        self.rejected({'jsd.engine': self.engine})

    def test_corrupt_payload(self):
        write(self.package / 'files/jsd.editor/build.sh', 'corrupt')
        self.rejected({'jsd.engine': self.engine})

    def test_duplicate_manifest_file(self):
        self.data['components'][0]['files'].append(self.data['components'][0]['files'][0])
        self.save_manifest()
        self.rejected({'jsd.engine': self.engine})

    def test_path_traversal(self):
        self.data['components'][0]['files'][0]['path'] = '../escape'
        self.save_manifest()
        self.rejected({'jsd.engine': self.engine})

    def test_symlink_target(self):
        real = self.engine / 'build.sh'
        real.unlink();real.symlink_to(self.package / 'files/jsd.engine/build.sh')
        self.rejected({'jsd.engine': self.engine})

    def test_symlink_root(self):
        link = self.root / 'link';link.symlink_to(self.engine, target_is_directory=True)
        self.rejected({'jsd.engine': link})

    def test_mixed_identity(self):
        fixture(self.engine, 'jsd.engine', '0.5.2', True)
        meta = guard.read_json(self.engine / 'JSD_PROJECT.json');meta['version'] = '0.5.3'
        write(self.engine / 'JSD_PROJECT.json', json.dumps(meta))
        self.rejected({'jsd.engine': self.engine})

    def test_payload_internal_version_mismatch(self):
        p = self.package / 'files/jsd.engine/include/jsd/engine.hpp'
        write(p, 'inline constexpr const char *Version = "0.5.12";')
        for f in self.data['components'][0]['files']:
            if f['path'] == 'include/jsd/engine.hpp':f['sha256'] = guard.digest(p)
        self.save_manifest();self.rejected({'jsd.engine': self.engine})

    def test_rollback_across_projects(self):
        before = tree(self.engine), tree(self.editor)
        real_replace = update.os.replace
        calls = []
        def failing_replace(a, b):
            calls.append(b)
            if len(calls) == 8:raise OSError('simulated write failure')
            return real_replace(a, b)
        with patch.object(update.os, 'replace', side_effect=failing_replace):
            with self.assertRaises(OSError):self.updated()
        for root in (self.engine, self.editor):
            shutil.rmtree(root / '.jsd-update-backups')
            (root / '.jsd-operation.lock').unlink()
        self.assertEqual(before, (tree(self.engine), tree(self.editor)))

    def test_lock_prevents_update(self):
        with guard.operation_lock(self.engine):
            self.rejected({'jsd.engine': self.engine})

    def test_journal_blocks_build_and_update(self):
        self.updated()
        write(self.engine / '.jsd-update-journal.json', '{}')
        with self.assertRaises(guard.GuardError):guard.source(self.engine, 'jsd.engine')
        self.rejected({'jsd.engine': self.engine})

    def test_wrong_build_cache_before_tools(self):
        self.updated()
        out = self.engine / 'build/release'
        write(out / 'CMakeCache.txt', f'CMAKE_HOME_DIRECTORY:INTERNAL={self.editor}\nCMAKE_PROJECT_NAME:STATIC=JSDStudio\n')
        with patch.object(build.subprocess, 'run') as run:
            with self.assertRaises(guard.GuardError):build.run(self.engine, 'jsd.engine', '0.5.3', [])
            run.assert_not_called()

    def test_future_build_cache(self):
        self.updated();out = self.engine / 'build/release'
        write(out / 'CMakeCache.txt', f'CMAKE_HOME_DIRECTORY:INTERNAL={self.engine}\nCMAKE_PROJECT_NAME:STATIC=JSDGameEngine\nCMAKE_PROJECT_VERSION:STATIC=0.6.0\n')
        with self.assertRaises(guard.GuardError):build.preflight(self.engine, 'jsd.engine', '0.5.3', [], {})

    def test_bad_prefix_before_tools(self):
        self.updated()
        with patch.dict(os.environ, {'JSD_INSTALL_PREFIX': str(self.editor)}), patch.object(build.subprocess, 'run') as run:
            with self.assertRaises(guard.GuardError):build.run(self.engine, 'jsd.engine', '0.5.3', [])
            run.assert_not_called()

    def test_redirecting_option_before_tools(self):
        self.updated()
        for arg in ('-B/other', '-DCMAKE_INSTALL_PREFIX=/other', '-DCMAKE_HOME_DIRECTORY=/other'):
            with patch.object(build.subprocess, 'run') as run:
                with self.assertRaises(guard.GuardError):build.run(self.engine, 'jsd.engine', '0.5.3', ['release', arg])
                run.assert_not_called()

    def test_clean_all_preflight_no_delete(self):
        self.updated();write(self.engine / 'build/sentinel', 'keep')
        before = tree(self.engine)
        with patch.dict(os.environ, {'JSD_BUILD_DIR': str(self.editor)}):
            with self.assertRaises(guard.GuardError):build.run(self.engine, 'jsd.engine', '0.5.3', ['clean'])
        self.assertEqual(before, tree(self.engine))

    def test_clean_preserves_sources_and_unrelated_outputs(self):
        self.updated();write(self.engine / 'build/sentinel', 'remove');write(self.engine / 'prebuilds/archive', 'remove')
        write(self.engine / 'lib/other.a', 'keep');write(self.engine / 'lib/libjsd-game-engine.a', 'remove')
        write(self.engine / 'lib/libJolt.a', 'remove');write(self.engine / 'include/SDL2/SDL.h', 'remove');write(self.engine / 'include/Jolt/Jolt.h', 'remove')
        before = (self.engine / 'include/jsd/engine.hpp').read_bytes()
        build.run(self.engine, 'jsd.engine', '0.5.3', ['clean'])
        self.assertFalse((self.engine / 'build').exists());self.assertFalse((self.engine / 'prebuilds').exists())
        self.assertTrue((self.engine / 'lib/other.a').exists());self.assertFalse((self.engine / 'lib/libjsd-game-engine.a').exists())
        self.assertEqual(before, (self.engine / 'include/jsd/engine.hpp').read_bytes())
        self.assertFalse((self.engine / 'lib/libJolt.a').exists());self.assertFalse((self.engine / 'include/SDL2').exists());self.assertFalse((self.engine / 'include/Jolt').exists())

    def test_editor_dependency_api_and_version(self):
        self.updated()
        editor = guard.source(self.editor, 'jsd.editor')
        for ver, api in [('0.5.2', 1), ('0.6.0', 1), ('0.5.3', 2)]:
            with self.assertRaises(guard.GuardError):guard.compatible(dict(project_id='jsd.engine', version=ver, api_version=api), editor)
        guard.compatible(dict(project_id='jsd.engine', version='0.5.12', api_version=1), editor)

    def test_unmarked_and_corrupt_sdk(self):
        self.updated();prefix = self.root / 'sdk';config = prefix / 'lib/cmake/JSDGameEngine'
        config.mkdir(parents=True)
        with self.assertRaises(guard.GuardError):guard.sdk(config)
        for rel in ('include/jsd/engine.hpp', 'include/frontier/renderer.hpp', 'lib/cmake/JSDGameEngine/JSDGameEngineConfig.cmake', 'lib/cmake/JSDGameEngine/JSDGameEngineConfigVersion.cmake', 'lib/libjsd-game-engine.so.0.5.3'):
            write(prefix / rel, 'sdk file')
        meta = dict(guard.source(self.engine, 'jsd.engine'), format='jsd-install', kind='sdk', files={p.relative_to(prefix).as_posix(): guard.digest(p) for p in prefix.rglob('*') if p.is_file()})
        meta['symlinks'] = {'lib/libjsd-game-engine.so': 'libjsd-game-engine.so.0', 'lib/libjsd-game-engine.so.0': 'libjsd-game-engine.so.0.5.3'}
        for rel, target in meta['symlinks'].items():(prefix / rel).symlink_to(target)
        write(prefix / 'share/jsd-game-engine/INSTALL.json', json.dumps(meta));guard.sdk(config)
        write(prefix / 'include/jsd/engine.hpp', 'corrupt')
        with self.assertRaises(guard.GuardError):guard.sdk(config)


    def test_symlink_backup_directory(self):
        (self.engine / '.jsd-update-backups').symlink_to(self.editor, target_is_directory=True)
        self.rejected({'jsd.engine': self.engine})

    def test_output_symlink_escape(self):
        self.updated();(self.engine / 'lib').mkdir()
        (self.engine / 'lib/cmake').symlink_to(self.editor, target_is_directory=True)
        with self.assertRaises(guard.GuardError):build.preflight(self.engine, 'jsd.engine', '0.5.3', [], {})

    def test_clean_foreign_nested_cache(self):
        self.updated()
        write(self.engine / 'build/foreign/CMakeCache.txt', f'CMAKE_HOME_DIRECTORY:INTERNAL={self.editor}\nCMAKE_PROJECT_NAME:STATIC=JSDStudio\n')
        before = tree(self.engine)
        with self.assertRaises(guard.GuardError):build.run(self.engine, 'jsd.engine', '0.5.3', ['clean'])
        self.assertEqual(before, tree(self.engine))

    def test_destdir_rejected_before_tools(self):
        self.updated()
        with patch.dict(os.environ, {'DESTDIR': str(self.root / 'foreign')}), patch.object(build.subprocess, 'run') as run:
            with self.assertRaises(guard.GuardError):build.run(self.engine, 'jsd.engine', '0.5.3', [])
            run.assert_not_called()

    def test_direct_cmake_wrong_version_before_project(self):
        self.updated()
        tools = Path(__file__).resolve().parents[1] / 'tools'
        for name in ('project_guard.py',):
            (self.engine / 'tools').mkdir(exist_ok=True);shutil.copy2(tools / name, self.engine / 'tools' / name)
        helper = Path(__file__).resolve().parents[1] / 'cmake/ProjectGuards.cmake'
        (self.engine / 'cmake').mkdir();shutil.copy2(helper, self.engine / 'cmake/ProjectGuards.cmake')
        write(self.engine / 'CMakeLists.txt', 'cmake_minimum_required(VERSION 3.21)\ninclude(cmake/ProjectGuards.cmake)\njsd_guard_project(jsd.engine 0.5.12)\nproject(JSDGameEngine VERSION 0.5.3 LANGUAGES NONE)\nfile(WRITE "${CMAKE_BINARY_DIR}/reached-project" "bad")\n')
        cmake = os.environ.get('CMAKE', 'cmake')
        result = subprocess.run([cmake, '-S', str(self.engine), '-B', str(self.engine / 'build/direct')], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Expected jsd.engine 0.5.12', result.stdout + result.stderr)
        self.assertFalse((self.engine / 'build/direct/reached-project').exists())

    def test_generator_validates_future_package(self):
        self.updated()
        import make_update
        future = self.root / 'future'
        fixture(future, 'jsd.engine', '0.5.4')
        write(future / 'build.sh', 'future build\n')
        out = self.root / 'future-update';out.mkdir()
        component = make_update.component(self.engine, future, 'jsd.engine', out)
        write(out / 'manifest.json', json.dumps(dict(format='jsd-update', schema=2, package_id='future-test', components=[component])))
        update.manifest(out)
        update.apply(out, {'jsd.engine': self.engine})
        self.assertEqual(guard.source(self.engine, 'jsd.engine')['version'], '0.5.4')


    def test_cmake_tool_version_before_dependency_setup(self):
        self.updated()
        response = subprocess.CompletedProcess(['cmake', '--version'], 0, 'cmake version 3.20.0\n', '')
        with patch.object(build.shutil, 'which', return_value='/fake/cmake'), patch.object(build.subprocess, 'run', return_value=response) as run:
            with self.assertRaises(guard.GuardError):build.run(self.engine, 'jsd.engine', '0.5.3', ['release'])
            self.assertEqual(run.call_count, 1)
            self.assertEqual(run.call_args.args[0][-1], '--version')

if __name__ == '__main__':
    unittest.main()
