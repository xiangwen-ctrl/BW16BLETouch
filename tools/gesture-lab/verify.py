"""Run with Python, playwright and Chrome: python verify.py

Reads the sibling HTML, drives Chrome's touch input, and writes a JSON report.
Timing boundary cases use explicit CDP event timestamps, independent of CPU load.
"""
import json
from datetime import datetime, timezone
from pathlib import Path
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parent
results = []


def run_layout(browser, width, height, dpr):
    page = browser.new_page(viewport={"width": width, "height": height},
                            device_scale_factor=dpr, has_touch=True)
    errors = []
    page.on("pageerror", lambda e: errors.append(str(e)))
    page.goto((root / "index.html").as_uri())
    cdp = page.context.new_cdp_session(page)
    # Input earlier than the navigation origin can be clamped to zero.
    clock = round(page.evaluate("performance.timeOrigin + performance.now()")) + 10
    initial_count = len(results)

    def touch(kind, points=(), ms=1):
        nonlocal clock
        clock += ms
        cdp.send("Input.dispatchTouchEvent", dict(type=kind, timestamp=clock/1000,
                 touchPoints=[dict(x=x, y=y, id=i+1) for i, (x,y) in enumerate(points)]))

    def center(selector, index):
        b = page.locator(selector).nth(index).bounding_box()
        return b["x"] + b["width"] / 2, b["y"] + b["height"] / 2

    def line(start, end, n=10, ms=15):
        for k in range(1, n+1):
            touch("touchMove", [tuple(a+(b-a)*k/n for a,b in zip(start,end))], ms)

    def begin(mode):
        page.locator("#"+mode).click()
        selector, n = (".row",3) if mode=="drag" else ("circle",4)
        return [center(selector,i) for i in range(n)]

    def check(name, ok, reason=None):
        status = page.locator("#status")
        assert status.get_attribute("data-ok")==str(ok).lower(), (name,status.inner_text(),page.evaluate("reports.at(-1)?.events.map(e=>e.ms)"))
        if reason: assert reason in status.inner_text(), (name,status.inner_text(),page.evaluate("reports.at(-1)?.events.map(e=>e.ms)"))
        results.append(dict(layout=[width,height,dpr],case=name,expected_accept=ok,
                            verdict=status.inner_text()))

    def drag(delay_ms, *, wrong=False):
        a,b,c = begin("drag")
        touch("touchStart",[a])
        target = b if wrong else c
        line(a,target,n=2,ms=delay_ms)
        touch("touchEnd")

    # First movement crosses slop and enters row B at exactly delay_ms.
    for ms,ok in [(649,False),(650,True),(651,True)]:
        drag(ms)
        recorded_ms=page.evaluate("""(() => {
            const events = reports.at(-1).events;
            return events.find(e => e.type === 'pointermove').timeStamp - events[0].timeStamp;
        })()""")
        assert abs(recorded_ms-ms)<=0.2,(ms,recorded_ms)
        # The event clock can land fractionally below a CDP timestamp request.
        # Verify the threshold using the input actually received by the page.
        ok = recorded_ms >= 650
        check("hold_"+str(ms),ok)
        results[-1]["first_move_ms"]=recorded_ms
        assert page.locator(".row").all_text_contents()==(["B","C","A"] if ok else ["A","B","C"])
    drag(700,wrong=True);check("wrong_drop",False)
    a,b,c=begin("drag")
    touch("touchStart",[a]); touch("touchMove",[c],700); touch("touchEnd")
    check("teleport_across_list",False)
    a,b,c=begin("drag")
    touch("touchStart",[a]); touch("touchMove",[(a[0]+6,a[1])],100)
    line(a,c,10,600);touch("touchEnd");check("small_initial_jitter",True)
    a,b,c=begin("drag")
    touch("touchStart",[a]);touch("touchMove",[(a[0]+15,a[1])],100)
    line(a,c,10,700);touch("touchEnd");check("excess_initial_jitter",False,"之前")
    a,b,c=begin("drag")
    touch("touchStart",[b]);line(b,c,5,700);touch("touchEnd");check("wrong_start",False,"起点")
    a,b,c=begin("drag")
    touch("touchStart",[a]);touch("touchStart",[a,center("h1",0)])
    line(a,c,10,700);touch("touchEnd");check("secondary_contact_outside_arena",False,"第二")

    def path(mode, hold=0, stop=0, variation=""):
        a,b,c,d=begin(mode)
        touch("touchStart",[a])
        if variation=="cancel":
            touch("touchCancel");return
        if variation=="cut":
            line(a,d);touch("touchEnd");return
        if variation=="sparse":
            for p in (b,c,d):touch("touchMove",[p],250)
            touch("touchEnd");return
        if variation=="early_lift":
            line(a,b);touch("touchEnd");return
        # Jump time between DOWN and first displacement, not between all samples.
        first=tuple(x+(y-x)*.1 for x,y in zip(a,b))
        touch("touchMove",[first],hold or 12);line(first,b)
        if variation=="outside":
            touch("touchMove",[(c[0],b[1])])
        first=tuple(x+(y-x)*.15 for x,y in zip(b,c))
        touch("touchMove",[first],stop or 12);line(first,c)
        line(c,d)
        if variation=="leave_end":line(d,c)
        touch("touchEnd")

    for v,ok in [("",True),("cut",False),("sparse",False),("early_lift",False),
                 ("outside",False),("leave_end",False),("cancel",False)]:
        path("path",variation=v);check("path_"+(v or "valid"),ok)
    for name,hold,stop,ok in [("valid",710,470,True),("no_hold",20,470,False),
                              ("no_stop",710,20,False),("recovery",710,470,True)]:
        path("relay",hold,stop);check("relay_"+name,ok)

    # Exact stop boundary: enter gate 2 at its center in one final short step,
    # then leave its radius in one move. Use mid-leg samples on every leg.
    for ms,ok in [(399,False),(400,True),(401,True)]:
        a,b,c,d=begin("relay");touch("touchStart",[a])
        mid=lambda u,v:tuple((x+y)/2 for x,y in zip(u,v))
        touch("touchMove",[mid(a,b)],700);touch("touchMove",[b],30)
        touch("touchMove",[mid(b,c)],ms);touch("touchMove",[c],30)
        touch("touchMove",[mid(c,d)],30);touch("touchMove",[d],30);touch("touchEnd")
        dwell=page.evaluate("reports.at(-1).dwellMs")
        assert abs(dwell-ms)<=0.2,(ms,dwell)
        check("stop_"+str(ms),dwell>=400)
        results[-1]["observed_dwell_ms"]=round(dwell,2)
    # Reinitializing the UI during a gesture must record an interrupted run
    # and must not let a late UP or timer complete the next run.
    a,b,c=begin("drag");touch("touchStart",[a])
    page.locator("#reset").evaluate("(button)=>button.click()")
    touch("touchEnd")
    prior=page.evaluate("({passed:reports.at(-1).passed, reason:reports.at(-1).reason})")
    assert not prior["passed"] and prior["reason"]=="测试被重置"
    assert page.locator("#status").get_attribute("data-ok")==""
    results.append(dict(layout=[width,height,dpr],case="reset_during_hold",expected_accept=False,verdict=prior["reason"]))
    a,b,c,d=begin("relay");touch("touchStart",[a])
    page.set_viewport_size({"width":height,"height":width})
    page.wait_for_function("document.querySelector('#status').dataset.ok==='false'")
    touch("touchEnd");check("resize_during_contact",False,"尺寸改变")
    page.set_viewport_size({"width":width,"height":height})
    page.wait_for_timeout(80)
    for n in range(5):
        path("path",variation="cancel");check("repeat_cancel_"+str(n),False)
        path("relay",710,470);check("repeat_recover_"+str(n),True)

    with page.expect_download() as ev: page.locator("#download").click()
    saved=json.loads(Path(ev.value.path()).read_text(encoding="utf8"))
    expected=results[initial_count:]
    assert len(saved["reports"])==len(expected)
    for r,test in zip(saved["reports"],expected):
        assert r["passed"]==test["expected_accept"] and r["source"]=="touch"
        assert r["events"][0]["type"]=="pointerdown"
        test["event_count"]=len(r["events"])
        if test["expected_accept"]:
            assert r["events"][-1]["type"]=="pointerup"
            assert len({e["id"] for e in r["events"]})==1
        if r["mode"]=="drag":
            assert r["order"]==(["B","C","A"] if test["expected_accept"] else ["A","B","C"])
    assert not errors, errors
    page.close()


if __name__=="__main__":
    with sync_playwright() as pw:
        browser=pw.chromium.launch(channel="chrome")
        for config in [(390,844,3),(720,1050,2.5),(1050,720,2)]:
            run_layout(browser,*config)
        report=dict(engine="Chrome CDP synthetic touch",browser=browser.version,
                    date=datetime.now(timezone.utc).isoformat(),passed=len(results),cases=results)
        (root/"automation-results.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
        print(f"{len(results)} cases passed across three viewports, including exported verdicts.")
        browser.close()
