/* TOiRE-76 UI skeleton (v2: body + independent indicator sprites)
 * Geometry comes from renders/components.json.
 * Browser prototype: falls back to an internal fake state when __JUCE__ is absent.
 */
(function () {
  "use strict";

  var SVGNS = "http://www.w3.org/2000/svg";
  var CFG = null;

  var state = {
    input: 0.2000, output: 0.6259, attack: 0.5000, release: 0.6660,
    ratio: 0,   // 0=4:1 1=8:1 2=12:1 3=20:1 4=ALL
    meter: 0,   // 0=GR 1=+8 2=+4 3=OFF
    gr: 0.0, level: 0.0, power: true, bypass: false
  };

  var BIG   = { body: "../renders/knob_body_big.png",   pointer: "../renders/knob_pointer.png", sizeCss: 96, pivotCss: 48 };
  var SMALL = { body: "../renders/knob_body_small.png", dot: "../renders/knob_dot.png",        sizeCss: 60, pivotCss: 30 };
  var DOT   = { sizeCss: 40, pivotCss: 20 };
  var NEEDLE = { sizeCss: 180, pivotCss: 90 };

  var KNOBS = [
    { id: "knob_input",   kind: "big",   key: "input" },
    { id: "knob_output",  kind: "big",   key: "output" },
    { id: "knob_attack",  kind: "small", key: "attack" },
    { id: "knob_release", kind: "small", key: "release" }
  ];

  var RATIO_KEYS = ["20", "12", "8", "4"];
  var RATIO_ROWS = [
    { id: "btn_ratio_20", ratio: 3 }, { id: "btn_ratio_12", ratio: 2 },
    { id: "btn_ratio_8",  ratio: 1 }, { id: "btn_ratio_4",  ratio: 0 }
  ];
  var ratioToggles = [false, false, false, true];
  var METER_ROWS = ["meter_gr", "meter_p8", "meter_p4", "meter_off"];
  var METER_KEYS = ["gr", "p8", "p4", "off"];

  var ASSET_GEO = {
    "btn_ratio_up.png":   { w: 32, h: 40, bx: 7.5, by: 5.5 },
    "btn_ratio_down.png": { w: 32, h: 40, bx: 7.5, by: 5.5 },
    "meter_col_up.png":   { w: 50, h: 150, bx: 15.5, by: 14.0 },
    "meter_btn_up.png":   { w: 32, h: 40, bx: 7.0, by: 5.5 }
  };

  function el(t, a) { var n = document.createElement(t); if (a) for (var k in a) n.setAttribute(k, a[k]); return n; }
  function sv(t, a) { var n = document.createElementNS(SVGNS, t); if (a) for (var k in a) n.setAttribute(k, a[k]); return n; }
  function find(id) { for (var i = 0; i < CFG.components.length; i++) if (CFG.components[i].id === id) return CFG.components[i]; return null; }
  function rad(d) { return d * Math.PI / 180; }
  function pt(cx, cy, r, deg) { return [cx + r * Math.cos(rad(deg)), cy + r * Math.sin(rad(deg))]; }

  /* ---------------- overlay ---------------- */
  function buildOverlay() {
    var o = document.getElementById("overlay"), m = CFG.markings;

    ["knob_input", "knob_output"].forEach(function (kid) {
      var c = find(kid), cx = c.pivot_px[0], cy = c.pivot_px[1];
      var marks = (kid === "knob_input") ? m.input_scale : m.output_scale;
      if (!marks || typeof marks === "string") marks = m.input_scale;
      marks.forEach(function (it) {
        var a = it.angle_cw - 90;
        var d = pt(cx, cy, 44, a);
        o.appendChild(sv("circle", { cx: d[0].toFixed(2), cy: d[1].toFixed(2), r: 1.1, fill: "#cfcfd2", opacity: .85 }));
        var p = pt(cx, cy, 52, a);
        var t = sv("text", { x: p[0].toFixed(2), y: p[1].toFixed(2), "text-anchor": "middle",
                             "dominant-baseline": "central", "font-size": 10.5, fill: "#dcdcdf",
                             "font-family": "Arial Narrow, Arial, sans-serif" });
        t.textContent = it.label; o.appendChild(t);
      });
    });

    ["knob_attack", "knob_release"].forEach(function (kid) {
      var c = find(kid), cx = c.pivot_px[0], cy = c.pivot_px[1];
      var marks = (kid === "knob_attack") ? m.attack_scale : m.release_scale;
      if (!marks || typeof marks === "string") marks = m.attack_scale;
      // 1/3/5/7 四个刻度必须等距：用首尾刻度角度均分 6 段（共 7 个刻度点）
      var a0 = marks[0].angle_cw, a1 = marks[marks.length - 1].angle_cw;
      if (a1 < a0) a1 += 360;
      var nSeg = 6, stp = (a1 - a0) / nSeg;
      for (var t = 0; t <= nSeg; t++) {
        var th = a0 + stp * t;
        var a = th - 90;
        var d = pt(cx, cy, 23, a);
        o.appendChild(sv("circle", { cx: d[0].toFixed(2), cy: d[1].toFixed(2), r: 0.95, fill: "#cfcfd2", opacity: .8 }));
        if (t % 2 === 0) {
          var p = pt(cx, cy, 31, a);
          var lb = sv("text", { x: p[0].toFixed(2), y: p[1].toFixed(2), "text-anchor": "middle",
                                "dominant-baseline": "central", "font-size": 9.5, fill: "#dcdcdf",
                                "font-family": "Arial Narrow, Arial, sans-serif" });
          lb.textContent = marks[t / 2].label; o.appendChild(lb);
        }
      }
    });

    if (m.captions) {
      Object.keys(m.captions).forEach(function (k) {
        var xy = m.captions[k];
        var t = sv("text", { x: xy[0], y: xy[1] + 7, "text-anchor": "middle", "font-size": 10,
                             "letter-spacing": "1.1", fill: "#c9c9cd",
                             "font-family": "Arial Narrow, Arial, sans-serif" });
        t.textContent = k; o.appendChild(t);
      });
    }
    (m.ratio_labels || []).forEach(function (it) {
      var t = sv("text", { x: m.ratio_label_x_px, y: it.y_px + 4, "text-anchor": "end",
                           "font-size": 11, fill: "#d6d6da", "font-family": "Arial Narrow, Arial, sans-serif" });
      t.textContent = it.label; o.appendChild(t);
    });
    (m.meter_labels || []).forEach(function (it) {
      var t = sv("text", { x: m.meter_label_x_px, y: it.y_px + 4, "text-anchor": "start",
                           "font-size": 11, fill: "#d6d6da", "font-family": "Arial Narrow, Arial, sans-serif" });
      t.textContent = it.label; o.appendChild(t);
    });

    buildVuScale(o, m.vu_scale);
  }

  function annulusPath(cx, cy, r0, r1, a0, a1) {
    var p0 = pt(cx, cy, r1, a0), p1 = pt(cx, cy, r1, a1), p2 = pt(cx, cy, r0, a1), p3 = pt(cx, cy, r0, a0);
    var large = (Math.abs(a1 - a0) > 180) ? 1 : 0;
    return "M " + p0[0].toFixed(2) + " " + p0[1].toFixed(2) +
      " A " + r1 + " " + r1 + " 0 " + large + " 1 " + p1[0].toFixed(2) + " " + p1[1].toFixed(2) +
      " L " + p2[0].toFixed(2) + " " + p2[1].toFixed(2) +
      " A " + r0 + " " + r0 + " 0 " + large + " 0 " + p3[0].toFixed(2) + " " + p3[1].toFixed(2) + " Z";
  }

  function buildVuScale(o, v) {
    if (!v) return;
    var cx = v.center_px[0], cy = v.center_px[1];
    var ink = "rgb(" + v.ink_rgb.join(",") + ")";
    var rOut = 61.0, rIn = 43.0;
    var rMinOut = 53.0;

    // no red band: positives are printed in red instead
    var labs = v.labels, i, a, b;
    // arc through ticks: black up to 0, red across the positive region
    var RED = "#c53a28";
    var iZero = 0;
    for (var z = 0; z < labs.length; z++) if ((labs[z].label + "") === "0") iZero = z;
    function arcSeg(ia, ib, col) {
      var pa = pt(cx, cy, 42.8, labs[ia].angle_image_deg);
      var pb = pt(cx, cy, 42.8, labs[ib].angle_image_deg);
      o.appendChild(sv("path", { d: "M " + pa[0].toFixed(2) + " " + pa[1].toFixed(2) + " A 42.8 42.8" +
                                    " 0 0 1 " + pb[0].toFixed(2) + " " + pb[1].toFixed(2),
                                 stroke: col, "stroke-width": 1.0, fill: "none" }));
    }
    arcSeg(0, iZero, ink);
    /* 红区：同一半径上把弧画粗，接在 0 之后（TOiRE-2A 的做法，比扇形干净） */
    (function () {
      var pa = pt(cx, cy, 42.8, labs[iZero].angle_image_deg);
      var pb = pt(cx, cy, 42.8, labs[labs.length - 1].angle_image_deg);
      o.appendChild(sv("path", { d: "M " + pa[0].toFixed(2) + " " + pa[1].toFixed(2) + " A 42.8 42.8" +
                                    " 0 0 1 " + pb[0].toFixed(2) + " " + pb[1].toFixed(2),
                                 stroke: "#8f1208", "stroke-width": 4.0, "stroke-linecap": "butt", fill: "none",
                                 opacity: .95 }));
    })();

    for (i = 0; i < labs.length; i++) {
      a = labs[i].angle_image_deg;
      var isPos = (labs[i].label + "").charAt(0) === "+";
      var col = isPos ? "#c53a28" : ink;
      var t1 = pt(cx, cy, rOut, a), t2 = pt(cx, cy, rIn, a);
      o.appendChild(sv("line", { x1: t1[0], y1: t1[1], x2: t2[0], y2: t2[1], stroke: col, "stroke-width": 1.3 }));
      // 数字放在自己刻度的正外侧，保证和刻度线同角、且贴近刻度
      var lp = pt(cx, cy, 66.0, a);
      var txt = sv("text", { x: lp[0].toFixed(2), y: lp[1].toFixed(2), "text-anchor": "middle",
                             "dominant-baseline": "central", "font-size": (isPos ? 6.2 : 7.4), fill: col, "font-weight": 400,
                             "font-family": "Arial Narrow, Arial, sans-serif" });
      var tang = (a + 90) * 0.55;   // UAD 印字只沿弧倾一半左右
      txt.setAttribute("transform", "rotate(" + tang.toFixed(1) + " " + lp[0].toFixed(2) + " " + lp[1].toFixed(2) + ")");
      txt.textContent = labs[i].label; o.appendChild(txt);
      if (i + 1 < labs.length) {
        b = labs[i + 1].angle_image_deg;
        var nMinor = (v.minor_ticks && v.minor_ticks.count_per_interval) || 1;
        for (var k = 1; k <= nMinor; k++) {
          var am = a + (b - a) * k / (nMinor + 1);
          var m1 = pt(cx, cy, rMinOut, am), m2 = pt(cx, cy, rIn, am);
          var nextPos = ("" + labs[i + 1].label).charAt(0) === "+";
          o.appendChild(sv("line", { x1: m1[0], y1: m1[1], x2: m2[0], y2: m2[1],
                                     stroke: (isPos && nextPos) ? "#c53a28" : ink, "stroke-width": 1.1, opacity: .9 }));
        }
      }
    }
    // percent scale: 均分在首尾刻度之间，保证每个数字都落在刻度正下方
    if (v.percent_scale && v.percent_scale.labels) {
      var pl = v.percent_scale.labels;
      var aFirst = labs[0].angle_image_deg, aLast = labs[labs.length - 1].angle_image_deg;  // 0% 对齐 -20，100% 对齐 +3
      for (var pi = 0; pi < pl.length; pi++) {
        var pa = aFirst + (aLast - aFirst) * pi / (pl.length - 1);
        var pq = pt(cx, cy, 37.0, pa);
        var pt2 = sv("text", { x: pq[0].toFixed(2), y: pq[1].toFixed(2), "text-anchor": "middle",
                               "dominant-baseline": "central", "font-size": 6.6, fill: ink, "font-weight": 400,
                               "font-family": "Arial Narrow, Arial, sans-serif" });
        pt2.textContent = pl[pi].label; o.appendChild(pt2);
      }
    }
    // VU 字样：放到表盘左上角 / 右上角（Kings 衬线）
    var faceC = find("meter_face");
    var fx = faceC ? faceC.box[0] : 645, fy = faceC ? faceC.box[1] : 42;
    var fw = faceC ? faceC.box[2] : 137, fh = faceC ? faceC.box[3] : 60;
    ["left", "right"].forEach(function (side) {
      var qx = (side === "left") ? (fx + 9) : (fx + fw - 9);
      var t = sv("text", { x: qx.toFixed(2), y: (fy + 9).toFixed(2), "text-anchor": "middle",
                           "dominant-baseline": "central", "font-size": 8.2, fill: ink,
                           "font-weight": 400, "font-family": "Kings, Georgia, serif" });
      t.textContent = "VU"; o.appendChild(t);
    });

    // 表盘内品牌字（Kings，小字号，放在刻度下方居中）
    var logo = sv("text", { x: cx.toFixed(2), y: 96.5, "text-anchor": "middle", "dominant-baseline": "central",
                            "font-size": 7.4, fill: "rgb(58,48,34)", "font-family": "Kings, Georgia, serif",
                            "letter-spacing": "0.6" });
    logo.textContent = "TOiRE";
    o.appendChild(logo);

    // 表盘下方三行铭牌（Kings）：型号 / 风格 / 类型，首尾两行大、中间小
    var nm = sv("text", { x: cx.toFixed(2), y: 134.0, "text-anchor": "middle", "dominant-baseline": "central",
                          "font-size": 15.0, fill: "#dcdcdf", "font-weight": 400,
                          "font-family": "Kings, Georgia, serif", "letter-spacing": "1.4" });
    nm.textContent = "TOiRE-76";
    o.appendChild(nm);

    var st = sv("text", { x: cx.toFixed(2), y: 145.5, "text-anchor": "middle", "dominant-baseline": "central",
                          "font-size": 8.6, fill: "#9a9aa0", "font-weight": 400,
                          "font-family": "Kings, Georgia, serif", "letter-spacing": "1.2" });
    st.textContent = "Toire style";
    o.appendChild(st);

    var ds = sv("text", { x: cx.toFixed(2), y: 157.0, "text-anchor": "middle", "dominant-baseline": "central",
                          "font-size": 9.4, fill: "#c6c6ca", "font-weight": 400,
                          "font-family": "Kings, Georgia, serif", "letter-spacing": "0.7" });
    ds.textContent = "FET COMPRESSOR / LIMITING AMPLIFIER";
    o.appendChild(ds);
  }

  /* big-knob pointer: white line over the disc only (r 24..40.5), SVG so it stays crisp */
  var POINTERS = [];
  var BODY_STRIPS = {};   // key -> {el, size, frames}（有旋转序列时替换静态 body）
  function buildKnobPointers() {
    var o = document.getElementById("overlay");
    [["knob_input", "input"], ["knob_output", "output"]].forEach(function (pair) {
      var c = find(pair[0]);
      var edge = sv("line", { stroke: "#111", "stroke-width": 2.5, "stroke-linecap": "round", opacity: .85 });
      var core = sv("line", { stroke: "#ffffff", "stroke-width": 1.75, "stroke-linecap": "round" });
      o.appendChild(edge); o.appendChild(core);
      POINTERS.push({ key: pair[1], cx: c.pivot_px[0], cy: c.pivot_px[1], edge: edge, core: core });
    });
  }
  function renderPointers() {
    for (var i = 0; i < POINTERS.length; i++) {
      var p = POINTERS[i], th = knobTheta({ id: "knob_" + p.key }, state[p.key]);
      var rad = (th - 90) * Math.PI / 180.0;      // theta_cw -> image angle
      var r0 = 24.0, r1 = 40.5;
      var x1 = p.cx + r0 * Math.cos(rad), y1 = p.cy + r0 * Math.sin(rad);
      var x2 = p.cx + r1 * Math.cos(rad), y2 = p.cy + r1 * Math.sin(rad);
      [p.edge, p.core].forEach(function (ln) {
        ln.setAttribute("x1", x1.toFixed(2)); ln.setAttribute("y1", y1.toFixed(2));
        ln.setAttribute("x2", x2.toFixed(2)); ln.setAttribute("y2", y2.toFixed(2));
      });
    }
  }

  /* ---------------- knobs: static body + rotating indicator ---------------- */
  function buildKnobs() {
    var host = document.getElementById("knobs");
    KNOBS.forEach(function (k) {
      var c = find(k.id), px = c.pivot_px[0], py = c.pivot_px[1];
      var isBig = (k.kind === "big");
      var bodySize = isBig ? BIG.sizeCss : SMALL.sizeCss;
      var wrap = el("div", { "class": "knob-wrap", "data-id": k.id });
      // the wrap is the hit area: same size as the knob, centred on the pivot
      wrap.style.left = (px - bodySize / 2) + "px";
      wrap.style.top  = (py - bodySize / 2) + "px";
      wrap.style.width = bodySize + "px";
      wrap.style.height = bodySize + "px";
      // body：直接用旋转序列（div + background-position 逐帧），失败才退回单帧静态图
      var staticSrc = isBig ? BIG.body : SMALL.body;
      var stripSrc  = (window.__ASSET_BASE__ || (typeof A !== "undefined" ? A : "../renders/")) +
                      (isBig ? "knob_body_big_61f.png" : "knob_body_small_61f.png");
      var bd = el("div", { "class": "knob-body" });
      bd.style.width = bodySize + "px";
      bd.style.height = bodySize + "px";
      bd.style.left = (bodySize / 2 - (isBig ? BIG.pivotCss : SMALL.pivotCss)) + "px";
      bd.style.top  = bd.style.left;
      bd.style.backgroundImage = "url(" + stripSrc + ")";
      bd.style.backgroundSize = bodySize + "px " + (bodySize * 61) + "px";
      bd.style.backgroundRepeat = "no-repeat";
      BODY_STRIPS[k.key] = { el: bd, size: bodySize, frames: 61 };
      (function (el2, fallback) {
        var probe = new Image();
        probe.onerror = function () {
          el2.style.backgroundImage = "url(" + fallback + ")";
          el2.style.backgroundSize = "100% 100%";
        };
        probe.src = stripSrc;
      })(bd, staticSrc);
      wrap.appendChild(bd);
      // indicator
      if (!isBig) {
        var ind = el("img", { "class": "knob-ind", src: SMALL.dot });
      var sz = isBig ? BIG.sizeCss : DOT.sizeCss;
      var pv = isBig ? BIG.pivotCss : DOT.pivotCss;
      ind.style.width = sz + "px"; ind.style.height = sz + "px";
      ind.style.left = (bodySize / 2 - pv) + "px"; ind.style.top = ind.style.left;
        ind.style.transformOrigin = pv + "px " + pv + "px";
        wrap.appendChild(ind);
      }
      host.appendChild(wrap);
      bindKnobDrag(wrap, k.key);
    });
  }

  function bindKnobDrag(node, key) {
    var dragging = false, lastY = 0;
    node.addEventListener("mousedown", function (e) { dragging = true; lastY = e.clientY; e.preventDefault(); });
    window.addEventListener("mousemove", function (e) {
      if (!dragging) return;
      var dy = lastY - e.clientY; lastY = e.clientY;
      state[key] = Math.min(1, Math.max(0, state[key] + dy / 180));
      render();
    });
    window.addEventListener("mouseup", function () { dragging = false; });
    node.addEventListener("wheel", function (e) {
      e.preventDefault();
      state[key] = Math.min(1, Math.max(0, state[key] - Math.sign(e.deltaY) * 0.02));
      render();
    }, { passive: false });
  }

  /* ---------------- buttons ---------------- */
  function placeSprite(host, assetName, visualBox, cls) {
    var g = ASSET_GEO[assetName];
    var d = el("div", { "class": "btn " + (cls || ""), "data-asset": assetName });
    d.style.left = (visualBox[0] - g.bx) + "px";
    d.style.top = (visualBox[1] - g.by) + "px";
    d.style.width = g.w + "px"; d.style.height = g.h + "px";
    d.style.backgroundImage = "url(../renders/" + assetName + ")";
    host.appendChild(d);
    return d;
  }

  function buildButtons() {
    var host = document.getElementById("buttons");
    RATIO_ROWS.forEach(function (row, idx) {
      var c = find(row.id);
      var d = placeSprite(host, "btn_ratio_up.png", c.visual_box, "ratio-btn");
      d.setAttribute("data-key", RATIO_KEYS[idx]);
      d.setAttribute("data-row", idx);
      d.addEventListener("click", function (e) {
        // 和 UAD 一致：单击=单选该档；Shift+单击任意键 = ALL（参数值 4）
        if (e.shiftKey) {
          state.ratio = 4;
        } else {
          state.ratio = RATIO_ROWS[idx].ratio;
        }
        if (typeof pushChoice === "function") pushChoice("ratio", state.ratio);
        render();
      });
    });
    // meter: four independent push buttons (radio)
    METER_ROWS.forEach(function (id, idx) {
      var c = find(id);
      var d = placeSprite(host, "meter_btn_up.png", c.visual_box || c.box, "meter-btn");
      d.setAttribute("data-row", idx);
      d.addEventListener("click", function () { state.meter = idx; render(); });
    });
  }

  /* ---------------- needle ---------------- */
  function needleTheta(t) {
    var c = find("meter_needle_single");
    return c.angle_min_deg + t * (c.angle_max_deg - c.angle_min_deg);
  }
  function knobTheta(k, v) {
    var c = find(k.id);
    /* 大旋钮（INPUT/OUTPUT）：panel 上「0」在 CW 端、「∞」在 CCW 端，
       UAD 里 norm=1 → 0 dB（最响）。所以大旋钮的角度要反过来算，
       顺时针（角度增大方向）对应 norm 变大。 */
    if (k.kind === "big")
      return c.angle_max_deg + v * (c.angle_min_deg - c.angle_max_deg);
    return c.angle_min_deg + v * (c.angle_max_deg - c.angle_min_deg);
  }


  /* VU 标准位置：dB 值 -> 指针角度（用印刷标签的角度表分段线性插值）
     GR 档：无压缩时指针停在「0」，压缩越大越往左（-20 方向）
     +4 / +8 档：读输出电平，无信号停在 -20（最左），+8 比 +4 少偏 4dB
     OFF 档：指针停在最左（-20） */
  /* DEV: ?switchTo=2&delay=800 用来验证切档动画 */
  var VU_POINTS = null;
  function devSwitch() {
    var q = location.search || "";
    var m = q.match(/[?&]switchTo=(\d)/), d = q.match(/[?&]delay=(\d+)/);
    if (!m) return;
    setTimeout(function () { state.meter = parseInt(m[1], 10); render(); }, d ? parseInt(d[1], 10) : 800);
  }

  function vuPoints() {
    if (VU_POINTS) return VU_POINTS;
    var v = CFG.markings.vu_scale;
    VU_POINTS = [];
    for (var i = 0; i < v.labels.length; i++) {
      var lab = parseFloat(String(v.labels[i].label));
      if (!isNaN(lab)) VU_POINTS.push([lab, v.labels[i].angle_image_deg + 90]);
    }
    VU_POINTS.sort(function (a, b) { return a[0] - b[0]; });
    return VU_POINTS;
  }
  function vuThetaForDb(db) {
    var p = vuPoints();
    if (db <= p[0][0]) return p[0][1];
    for (var i = 1; i < p.length; i++) {
      if (db <= p[i][0]) {
        var f = (db - p[i - 1][0]) / (p[i][0] - p[i - 1][0]);
        return p[i - 1][1] + f * (p[i][1] - p[i - 1][1]);
      }
    }
    return p[p.length - 1][1];
  }


  /* 指针动画：只在「切换 GR / +8 / +4 / OFF」时用弹簧-阻尼滑过去；
     同档内的数值变化直接落位（将来由 DSP 自己的 attack/release 弹道驱动）*/
  var NEEDLE_PHYS = { cur: null, vel: 0, target: 0, raf: 0, k: 260, c: 22, mode: -1, stop: false };

  function needleApply(deg) {
    var blade = document.getElementById("needle-blade");
    var shadow = document.getElementById("needle-shadow");
    if (blade) blade.style.transform = "rotate(" + deg.toFixed(2) + "deg)";
    if (shadow) shadow.style.transform = "rotate(" + deg.toFixed(2) + "deg)";
  }

  function needleSetTarget(thetaDeg, mode) {
    var p = NEEDLE_PHYS;
    if (p.cur === null) p.cur = thetaDeg;
    if (mode !== p.mode) {
      p.mode = mode;
      if (mode === 0) { p.k = 700; p.c = 38; }        // GR：约 0.35s
      else if (mode === 1 || mode === 2) { p.k = 420; p.c = 30; }  // +8/+4：约 0.45s
      else { p.k = 300; p.c = 28; }                   // OFF：约 0.55s
      p.target = thetaDeg;
      p.stop = false;
      if (!p.raf) needleLoop();
      return;
    }
    // 同档：直接落位，不做插值（但动画进行中一律忽略，避免被重复 render 打断）
    if (p.raf) return;
    p.target = thetaDeg;
    p.cur = thetaDeg;
    p.vel = 0;
    needleApply(p.cur);
  }

  function needleLoop() {
    NEEDLE_PHYS.raf = 1;
    var last = 0;
    function step(ts) {
      var p = NEEDLE_PHYS;
      if (p.stop) { p.raf = 0; p.stop = false; return; }
      if (!last) last = ts;
      var dt = Math.min(0.05, (ts - last) / 1000);
      last = ts;
      var a = -p.k * (p.cur - p.target) - p.c * p.vel;
      p.vel += a * dt;
      p.cur += p.vel * dt;
      needleApply(p.cur);
      var settled = Math.abs(p.vel) < 0.02 && Math.abs(p.cur - p.target) < 0.02;
      if (settled) { p.vel = 0; p.cur = p.target; needleApply(p.cur); p.raf = 0; return; }
      requestAnimationFrame(step);
    }
    requestAnimationFrame(step);
  }

  /* ---------------- render ---------------- */
  var DEBUG = /[?&]debug=1/.test(location.search) || !window.__JUCE__;
  function render() {
    renderPointers();
    KNOBS.forEach(function (k) {
      var wrap = document.querySelector('.knob-wrap[data-id="' + k.id + '"]');
      if (!wrap) return;
      var strip = BODY_STRIPS[k.key];
      if (strip) {
        var fr = (strip.frames - 1) - Math.round(state[k.key] * (strip.frames - 1));   // 序列帧序与指针方向相反，翻转
        strip.el.style.backgroundPositionY = (-fr * strip.size) + "px";
      }
      var ind = wrap.querySelector(".knob-ind");
      if (ind) ind.style.transform = "rotate(" + knobTheta(k, state[k.key]).toFixed(2) + "deg)";
    });

    var btn = document.querySelectorAll(".ratio-btn");
    for (var i = 0; i < btn.length; i++) {
      var down = (state.ratio === 4) ? true : (state.ratio === RATIO_ROWS[i].ratio);
      btn[i].style.backgroundImage = "url(../renders/btn_ratio_" + RATIO_KEYS[i] + (down ? "_down.png" : "_up.png") + ")";
    }
    var mbn = document.querySelectorAll(".meter-btn");
    for (var m = 0; m < mbn.length; m++) {
      var downM = (state.meter === m);
      mbn[m].style.backgroundImage = "url(../renders/meter_btn_" + METER_KEYS[m] + (downM ? "_down.png" : "_up.png") + ")";
    }

    // 指针位置按 VU 标准：GR 停 0、+4/+8 读电平、OFF 停最左
    var dbNow, mode = state.meter;
    if (!state.power) { dbNow = -20; }
    else if (mode === 0) { dbNow = 0 - (state.gr || 0) * 20; }              // GR：0 → -20
    else if (mode === 1) { dbNow = (-20 + (state.level || 0) * 23) - 8; } // +8
    else if (mode === 2) { dbNow = (-20 + (state.level || 0) * 23) - 4; } // +4
    else { dbNow = -20; }                                                    // OFF
    if (dbNow < -20) dbNow = -20;
    if (dbNow > 3) dbNow = 3;
    var t = (vuThetaForDb(dbNow) + 49.13) / 100.29;   // 映射回 0..1 供现有旋转代码使用
    if (t < 0) t = 0; if (t > 1) t = 1;
    var theta = needleTheta(t).toFixed(2);
    needleSetTarget(parseFloat(theta), state.meter);

    if (!DEBUG) { document.getElementById("hud").style.display = "none"; }
    document.getElementById("hud").textContent =
      "in " + state.input.toFixed(3) + "  out " + state.output.toFixed(3) +
      "  att " + state.attack.toFixed(3) + "  rel " + state.release.toFixed(3) + "\n" +
      "ratio " + ["4:1", "8:1", "12:1", "20:1", "ALL"][state.ratio] +
      "   meter " + ["GR", "+8", "+4", "OFF"][state.meter];
  }

  function buildNeedle() {
    var win = document.getElementById("vu-window");
    var c = find("meter_needle_single");
    var px = c.pivot_px[0], py = c.pivot_px[1];
    var box = find("meter_needle").box;   // window box 651,43,130,66
    [["needle-shadow", "../renders/needle_shadow.png"],
     ["needle-blade", "../renders/needle_blade.png"]].forEach(function (pair) {
      var im = el("img", { id: pair[0], src: pair[1] });
      im.style.width = NEEDLE.sizeCss + "px"; im.style.height = NEEDLE.sizeCss + "px";
      im.style.left = (px - NEEDLE.pivotCss - box[0]) + "px";
      im.style.top  = (py - NEEDLE.pivotCss - box[1]) + "px";
      im.style.transformOrigin = NEEDLE.pivotCss + "px " + NEEDLE.pivotCss + "px";
      win.appendChild(im);
    });
  }


  /* ---------------- boot ---------------- */
  function boot(cfg) {
    CFG = cfg;
    (function () {
      var q = location.search || "";
      function num(k) { var m = q.match(new RegExp("[?&]" + k + "=([0-9.]+)")); return m ? parseFloat(m[1]) : null; }
      var mv = num("meter"); if (mv !== null) state.meter = Math.max(0, Math.min(3, Math.round(mv)));
      var gv = num("gr"); if (gv !== null) state.gr = Math.max(0, Math.min(1, gv));
      var lv = num("level"); if (lv !== null) state.level = Math.max(0, Math.min(1, lv));
      var rv = num("ratio"); if (rv !== null) state.ratio = Math.max(0, Math.min(4, Math.round(rv)));
    })();
    // 开发用：?meter=0..3&gr=0..1&level=0..1 覆盖初始状态
    (function () {
      var q = location.search || "";
      function num(k) { var m = q.match(new RegExp("[?&]" + k + "=([0-9.]+)")); return m ? parseFloat(m[1]) : null; }
      var mv = num("meter"); if (mv !== null) state.meter = Math.max(0, Math.min(3, Math.round(mv)));
      var gv = num("gr"); if (gv !== null) state.gr = Math.max(0, Math.min(1, gv));
      var lv = num("level"); if (lv !== null) state.level = Math.max(0, Math.min(1, lv));
      var rv = num("ratio"); if (rv !== null) state.ratio = Math.max(0, Math.min(4, Math.round(rv)));
    })();
    devSwitch();
    buildOverlay(); buildKnobPointers(); buildKnobs(); buildButtons(); buildNeedle(); render();
    window.TOIRE76 = { state: state, render: render };
  }

  var inline = window.TOIRE76_CFG || null;
  if (inline) { boot(inline); return; }
  fetch("../renders/components.json").then(function (r) { return r.json(); }).then(boot)
    .catch(function (e) {
      if (!DEBUG) { document.getElementById("hud").style.display = "none"; }
    document.getElementById("hud").textContent = "load error: " + e;
    });
})();
