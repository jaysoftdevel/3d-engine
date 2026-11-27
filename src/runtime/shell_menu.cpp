#include <jsd/game_shell.hpp>
#include <jsd/detail/shell_font.hpp>
#include <algorithm>
#include <cmath>

namespace jsd::shell {
namespace {
void drawBackdrop(Canvas& r, float w, float h){
    r.gradient({0,0,w,h},{.012f,.023f,.043f,1},{.066f,.111f,.141f,1});
    for(unsigned i=0;i<76;++i){
        const unsigned seed=i*15485863u+641u;
        const float x=float((seed*7u+13u)%1997)/1997.0f*(w-2.0f);
        const float y=float((seed*43u+23u)%1597)/1597.0f*h*.83f;
        const float size=i%13==0?2.0f:1.0f;
        const rf::Color color{.77f,.86f,.95f,.19f+.11f*(i%4)};
        r.gradient({x,y,size,size},color,color);
    }

    const float radius=std::min(h*.33f,w*.22f);
    const Point center{w*.76f,h*.45f};
    constexpr int segments=96;
    std::vector<rf::V3> shape;
    shape.reserve(segments*6);
    const auto disc_point=[&](int i){
        const float angle=6.28318530718f*i/segments;
        return rf::V3{center.x+radius*std::cos(angle),center.y+radius*std::sin(angle),0};
    };
    for(int i=0;i<segments;++i)
        shape.insert(shape.end(),{rf::V3{center.x,center.y,0},disc_point(i),disc_point(i+1)});
    r.triangles(shape,{.044f,.070f,.091f,.98f});
    const auto crescent=[&](int i,float t){
        const float angle=3.14159265359f*i/segments-1.57079632679f;
        const float latitude=std::sin(angle);
        const float half=radius*std::max(0.f,std::cos(angle));
        return rf::V3{center.x+half*std::lerp(.43f+.30f*latitude,1.f,t),center.y+radius*latitude,0};
    };
    constexpr std::array<rf::Color,5> shades{{
        {.15f,.22f,.27f,1},{.20f,.28f,.32f,1},{.26f,.34f,.39f,1},
        {.31f,.40f,.44f,1},{.43f,.52f,.56f,1}
    }};
    constexpr float edges[]{0,.28f,.53f,.75f,.985f,1};
    for(std::size_t band=0;band<shades.size();++band){
        shape.clear();
        for(int i=0;i<segments;++i){
            const rf::V3 a=crescent(i,edges[band]),b=crescent(i,edges[band+1]);
            const rf::V3 c=crescent(i+1,edges[band+1]),d=crescent(i+1,edges[band]);
            shape.insert(shape.end(),{a,b,c,a,c,d});
        }
        r.triangles(shape,shades[band]);
    }

    const auto ridge=[&](float baseline,float height,rf::Color color){
        constexpr int segments=40;
        const auto elevation=[&](float u){
            return baseline-height*(.53f+.24f*std::sin(u*19.0f)+.16f*std::sin(u*33.0f+2.0f));
        };
        std::vector<rf::V3> triangles;
        triangles.reserve(segments*6);
        for(int i=0;i<segments;++i){
            const float a=float(i)/segments,b=float(i+1)/segments;
            const rf::V3 first{a*w,elevation(a),0},last{b*w,elevation(b),0};
            const rf::V3 bottom_left{a*w,h,0},bottom_right{b*w,h,0};
            triangles.insert(triangles.end(),{first,last,bottom_right,first,bottom_right,bottom_left});
        }
        r.triangles(triangles,color);
    };
    ridge(h*.94f,h*.12f,{.052f,.078f,.100f,1});
    ridge(h*1.01f,h*.15f,{.028f,.047f,.062f,1});
}
void fill(Canvas& canvas, Rect box, rf::Color color) {
    canvas.gradient(box, color, color);
}

void border(Canvas& canvas, Rect r, rf::Color color, float width) {
    const Point a{r.x, r.y}, b{r.x+r.w, r.y}, c{r.x+r.w, r.y+r.h}, d{r.x, r.y+r.h};
    canvas.line(a, b, color, width);
    canvas.line(b, c, color, width);
    canvas.line(c, d, color, width);
    canvas.line(d, a, color, width);
}

Point measure(std::string_view text) {
    std::size_t longest=0, current=0, lines=1;
    for (char c : text) {
        if (c == '\n') {
            longest=std::max(longest,current);
            current=0;
            ++lines;
        } else {
            ++current;
        }
    }
    return {float(std::max(longest,current))*detail::glyphAdvance,
            detail::glyphBodyHeight+float(lines-1)*detail::lineAdvance};
}

void label(Canvas& canvas, Rect box, std::string_view text, float desired, rf::Color color, bool centered=false) {
    if (text.empty() || box.w <= 0 || box.h <= 0)
        return;
    const auto extent=measure(text);
    const float scale=std::min({desired, box.w/std::max(extent.x,1.f), box.h/extent.y});
    const Point p=centered ? Point{box.x+(box.w-extent.x*scale)*.5f, box.y+(box.h-extent.y*scale)*.5f}
                           : Point{box.x,box.y};
    canvas.text(p,text,scale,color);
}
} // namespace

TitleItems::TitleItems(TitleOptions options) {
    const auto add=[&](std::string_view label, TitleAction action) {
        entries[count]={label};
        actions[count++]=action;
    };
    if (options.activeSession) {
        add("RESUME SESSION",TitleAction::Resume);
        if (options.savesSupported)
            add("SAVE GAME",TitleAction::Save);
    } else if (options.savesSupported && options.hasRecentSave) {
        add("CONTINUE",TitleAction::Continue);
    }
    add("NEW GAME",TitleAction::NewGame);
    if (options.savesSupported)
        add("LOAD GAME",TitleAction::Load);
    add("SETTINGS",TitleAction::Settings);
    add("CREDITS",TitleAction::Credits);
    add("EXIT",TitleAction::Exit);
}

std::optional<std::size_t> Menu::draw(Canvas& canvas, float w, float h, const Page& page, const Input& in,
                                      const Theme& theme) {
    if (w <= 0 || h <= 0)
        return {};
    drawBackdrop(canvas,w,h);
    const auto enabled=[&](std::size_t index) {
        return index < page.items.size() && page.items[index].enabled;
    };
    const auto first=std::find_if(page.items.begin(),page.items.end(),[](const auto& item){return item.enabled;});
    const auto preferred=enabled(page.preferred) ? page.preferred : std::size_t(first-page.items.begin());
    if (keyboardFocus && !enabled(*keyboardFocus))
        keyboardFocus.reset();
    if (preferred < page.items.size() && (in.up || in.down)) {
        const auto count=page.items.size();
        auto index=keyboardFocus.value_or(preferred);
        do {
            if (in.up)
                index=index==0 ? count-1 : index-1;
            else
                index=index+1==count ? 0 : index+1;
        } while (!enabled(index));
        keyboardFocus=index;
    }

    const bool split=w>=750.f && !page.context.empty();
    const float margin=std::min(22.f,w*.055f);
    const float panelWidth=std::min(split?870.f:408.f,w-2*margin);
    const float panelX=split?std::max(margin,std::min(w*.11f,w-panelWidth-margin)):(w-panelWidth)*.5f;
    const float rows=float(std::max<std::size_t>(page.items.size(),1));
    const bool compact=h<720;
    const float naturalButton=compact?36.f:44.f, naturalGap=compact?7.f:10.f;
    float panelHeight=rows*naturalButton+(rows-1)*naturalGap+36;
    const float scale=std::min(1.f,h/(panelHeight+148.f));
    panelHeight*=scale;
    const float padding=18*scale, buttonHeight=naturalButton*scale, gap=naturalGap*scale;
    const Rect panel{panelX,std::max(103*scale,(h-panelHeight)*.5f),panelWidth,panelHeight};
    const auto font=[&](float size){return std::max(1.f,size*theme.fontScale)*scale;};
    label(canvas,{panel.x,panel.y-79*scale,panel.w,62*scale},page.title,font(5.4f),theme.text);
    fill(canvas,panel,theme.panel);
    border(canvas,panel,theme.border,1);
    fill(canvas,{panel.x,panel.y,panel.w,2},theme.accent);

    const float actionsWidth=split?std::min(398.f,panel.w*.51f):std::max(0.f,panel.w-2*padding);
    std::optional<std::size_t> selected;
    for (std::size_t i=0;i<page.items.size();++i) {
        const Rect box{panel.x+padding,panel.y+padding+float(i)*(buttonHeight+gap),actionsWidth,buttonHeight};
        const auto& item=page.items[i];
        const bool pointed=in.pointer.x>=box.x && in.pointer.x<=box.x+box.w &&
                           in.pointer.y>=box.y && in.pointer.y<=box.y+box.h;
        const bool focused=item.enabled && (pointed || keyboardFocus==i);
        const bool primary=item.enabled && i==preferred;
        const auto background=primary?rf::Color{.10f,.27f,.28f,1}:theme.control;
        const auto hover=primary?rf::Color{.16f,.40f,.40f,1}:theme.hover;
        fill(canvas,box,item.enabled?(focused?hover:background):rf::Color{.07f,.075f,.085f,1});
        border(canvas,box,focused||primary?theme.accent:theme.border,2);
        const float inset=(box.w<60.f?4.f:10.f)*scale;
        label(canvas,{box.x+inset,box.y+6*scale,box.w-2*inset,box.h-12*scale},
              item.label,2.30f*scale,item.enabled?theme.text:theme.muted,true);
        if (item.enabled && pointed && in.click) {
            keyboardFocus.reset();
            selected=i;
        }
    }

    if (split) {
        const Rect box{panel.x+actionsWidth+2*padding,panel.y+padding,panel.w-actionsWidth-3*padding,panel.h-2*padding};
        canvas.gradient(box,{.047f,.078f,.099f,1},{.033f,.053f,.072f,1});
        const float contentTop=box.y+padding, contentBottom=box.y+box.h-padding;
        const float contextHeight=std::max(0.f,contentBottom-contentTop);
        float extent=0;
        for (const auto& line : page.context)
            extent=std::max(extent,line.offset+23.f);
        const float contextScale=extent>0?std::min(scale,contextHeight/extent):scale;
        for (const auto& line : page.context)
            label(canvas,{box.x+padding,contentTop+line.offset*contextScale,box.w-2*padding,23*contextScale},
                  line.text,std::max(1.f,line.scale*theme.fontScale)*contextScale,line.muted?theme.muted:theme.text);
    }

    const float footerWidth=std::min(151.f,w*.34f);
    label(canvas,{w-16-footerWidth,h-31,footerWidth,18},page.footer,font(.67f),theme.muted);
    label(canvas,{16,h-31,w-footerWidth-47,18},page.status,font(.73f),theme.muted);
    const auto key=keyboardFocus.value_or(preferred);
    if (!selected && in.select && !in.click && enabled(key))
        selected=key;
    return selected;
}

} // namespace jsd::shell
