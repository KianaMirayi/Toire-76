/* =====================================================================
   TOiRE-76 WebUI app.js —— JUCE 8 WebView2 版（全 ES5）

   派生自 web/app.js（v6 分层合成版），改动只有五处：
     1. 资产路径 ../renders/ -> assets/
     2. 配置来自 ui-config.js（window.TOIRE76_CFG），不读外部 JSON
        （WebView2 用 file:// 打开页面时，XHR 读相邻目录会被同源策略挡住 -> 白屏）
     3. 参数走 JUCE relay：连续参数 / Choice 用 slider state，
        power / bypass 用 toggle state；没有 __JUCE__ 时装一个 mock 后端
     4. 全 ES5：var / function，不用箭头函数、模板字符串、块级声明、幂运算符、可选链
     5. 加了 fit()（按窗口等比缩放）、#errors 红条、调试 HUD、POWER/BYPASS 开关

   协议（和 JUCE 8.0.7 的 juce_WebControlRelayEvents.h 一致）：
     C++ -> JS : {eventType:"propertiesChanged", start, end, skew, interval, name, ...}
                 {eventType:"valueChanged", value:<去归一化后的值>}
                 {eventType:"valueChanged", value:<bool>}     // toggle
     JS -> C++ : {eventType:"valueChanged", value}
                 {eventType:"sliderDragStarted"} / {eventType:"sliderDragEnded"}
                 {eventType:"requestInitialUpdate"}
   ===================================================================== */
(function () {
  "use strict";

  var SVGNS = "http://www.w3.org/2000/svg";
  var A = "assets/";

  /* ---------------- 0. 错误红条（DAW 里没有 DevTools） ---------------- */
  function showError(msg) {
    var el = document.getElementById("errors");
    if (!el) return;
    el.style.display = "block";
    el.textContent += (el.textContent ? "\n" : "") + msg;
  }

  window.onerror = function (m, s, l) {
    showError(m + "  @  " + (s || "") + ":" + (l || 0));
    return false;
  };

  var CFG = window.TOIRE76_CFG || null;

  /* =====================================================================
     1. JUCE 8 前端桥（ES5 重实现官方 juce-framework-frontend 的等价物）

     官方前端框架是 ES module，WebView2 里用传统 <script> 更稳，所以这里
     按同一套事件协议自己实现 getSliderState / getToggleState。
     JUCE 的 lowLevelIntegrationsScript 已经注入了 window.__JUCE__.backend，
     我们只补上层对象；没有后端（直接用浏览器打开）时装 mock。
     ===================================================================== */
  function makeListeners() {
    var fns = [];
    return {
      addListener: function (fn) { fns.push(fn); return fns.length - 1; },
      removeListener: function (i) { if (i >= 0 && i < fns.length) fns[i] = null; },
      call: function (arg) {
        for (var i = 0; i < fns.length; i++) {
          if (!fns[i]) continue;
          try { fns[i](arg); } catch (e) { showError("listener: " + e); }
        }
      }
    };
  }

  /* ---- slider state ---- */
  function SliderState(name) {
    this.name = name;
    this.identifier = "__juce__slider" + name;
    this.scaledValue = 0;
    this.properties = {
      start: 0, end: 1, skew: 1, interval: 0,
      name: name, label: "", numSteps: 1000, parameterIndex: -1
    };
    this.valueChangedEvent = makeListeners();
    this.propertiesChangedEvent = makeListeners();

    var self = this;
    window.__JUCE__.backend.addEventListener(this.identifier, function (ev) {
      self.handleEvent(ev);
    });

    /* 页面加载完成后再要一次初值（C++ 建 attachment 时页面还没加载，
       那次 initial update 会被丢掉） */
    window.__JUCE__.backend.emitEvent(this.identifier, { eventType: "requestInitialUpdate" });
  }

  SliderState.prototype.handleEvent = function (ev) {
    if (!ev) return;

    if (ev.eventType === "valueChanged") {
      if (typeof ev.value === "number") {
        this.scaledValue = ev.value;
        this.valueChangedEvent.call(undefined);
      }
    } else if (ev.eventType === "propertiesChanged") {
      if (typeof ev.start === "number") this.properties.start = ev.start;
      if (typeof ev.end === "number") this.properties.end = ev.end;
      if (typeof ev.skew === "number" && ev.skew > 0) this.properties.skew = ev.skew;
      if (typeof ev.interval === "number") this.properties.interval = ev.interval;
      if (ev.name) this.properties.name = String(ev.name);
      if (ev.label) this.properties.label = String(ev.label);
      this.propertiesChangedEvent.call(undefined);
    }
  };

  SliderState.prototype.getScaledValue = function () {
    return this.scaledValue;
  };

  SliderState.prototype.getNormalisedValue = function () {
    var p = this.properties;
    var span = p.end - p.start;
    if (span === 0) return 0;
    var n = (this.scaledValue - p.start) / span;
    if (n < 0) n = 0;
    if (n > 1) n = 1;
    return Math.pow(n, p.skew);
  };

  SliderState.prototype.normalisedToScaledValue = function (n) {
    var p = this.properties;
    if (typeof n !== "number" || !isFinite(n)) return this.scaledValue;
    if (n < 0) n = 0;
    if (n > 1) n = 1;
    return Math.pow(n, 1 / p.skew) * (p.end - p.start) + p.start;
  };

  SliderState.prototype.snapToLegalValue = function (v) {
    var p = this.properties;
    if (!p.interval) return v;
    var snapped = p.start + p.interval * Math.floor((v - p.start) / p.interval + 0.5);
    if (snapped < p.start) snapped = p.start;
    if (snapped > p.end) snapped = p.end;
    return snapped;
  };

  SliderState.prototype.setScaledValue = function (v) {
    this.scaledValue = this.snapToLegalValue(v);
    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: "valueChanged", value: this.scaledValue
    });
  };

  SliderState.prototype.setNormalisedValue = function (n) {
    this.setScaledValue(this.normalisedToScaledValue(n));
  };

  SliderState.prototype.sliderDragStarted = function () {
    window.__JUCE__.backend.emitEvent(this.identifier, { eventType: "sliderDragStarted" });
  };

  SliderState.prototype.sliderDragEnded = function () {
    window.__JUCE__.backend.emitEvent(this.identifier, { eventType: "sliderDragEnded" });
  };

  /* ---- toggle state ---- */
  function ToggleState(name) {
    this.name = name;
    this.identifier = "__juce__toggle" + name;
    this.value = false;
    this.properties = { name: name, parameterIndex: -1 };
    this.valueChangedEvent = makeListeners();
    this.propertiesChangedEvent = makeListeners();

    var self = this;
    window.__JUCE__.backend.addEventListener(this.identifier, function (ev) {
      self.handleEvent(ev);
    });

    window.__JUCE__.backend.emitEvent(this.identifier, { eventType: "requestInitialUpdate" });
  }

  ToggleState.prototype.handleEvent = function (ev) {
    if (!ev) return;

    if (ev.eventType === "valueChanged") {
      this.value = !!ev.value;
      this.valueChangedEvent.call(undefined);
    } else if (ev.eventType === "propertiesChanged") {
      if (ev.name) this.properties.name = String(ev.name);
      this.propertiesChangedEvent.call(undefined);
    }
  };

  ToggleState.prototype.getValue = function () {
    return this.value;
  };

  ToggleState.prototype.setValue = function (newValue) {
    this.value = !!newValue;
    window.__JUCE__.backend.emitEvent(this.identifier, {
      eventType: "valueChanged", value: this.value
    });
  };

  /* ---- 浏览器调试用的假后端（没有 __JUCE__ 时安装） ---- */
  function installMockBackend() {
    var listenersById = {};

    var sliderDefs = {
      input:   { start: 0, end: 1, skew: 1, interval: 0, value: 0.2000 },
      output:  { start: 0, end: 1, skew: 1, interval: 0, value: 0.6259 },
      attack:  { start: 0, end: 1, skew: 1, interval: 0, value: 0.5000 },
      release: { start: 0, end: 1, skew: 1, interval: 0, value: 0.6660 },
      ratio:   { start: 0, end: 4, skew: 1, interval: 1, value: 0 },
      meter:   { start: 0, end: 3, skew: 1, interval: 1, value: 0 }
    };
    var toggleDefs = { power: true, bypass: false };

    function fire(id, payload) {
      var arr = listenersById[id] || [];
      for (var i = 0; i < arr.length; i++) {
        try { arr[i](payload); } catch (e) { showError("mock listener: " + e); }
      }
    }

    window.__JUCE__ = {
      initialisationData: {
        __juce__platform: ["windows"],
        __juce__functions: [],
        __juce__sliders: ["input", "output", "attack", "release", "ratio", "meter"],
        __juce__toggles: ["power", "bypass"],
        __juce__comboBoxes: [],
        __juce__registeredGlobalEventIds: []
      },
      backend: {
        addEventListener: function (id, fn) {
          if (!listenersById[id]) listenersById[id] = [];
          listenersById[id].push(fn);
          return [id, listenersById[id].length - 1];
        },
        removeEventListener: function () {},
        emitEvent: function (id, payload) {
          if (!payload) return;

          var sid = String(id);
          var isSlider = sid.indexOf("__juce__slider") === 0;
          var isToggle = sid.indexOf("__juce__toggle") === 0;
          var name = "";
          if (isSlider) name = sid.substring("__juce__slider".length);
          else if (isToggle) name = sid.substring("__juce__toggle".length);

          if (isSlider) {
            var d = sliderDefs[name] || { start: 0, end: 1, skew: 1, interval: 0, value: 0 };
            if (payload.eventType === "requestInitialUpdate") {
              setTimeout(function () {
                fire(id, { eventType: "propertiesChanged", start: d.start, end: d.end,
                           skew: d.skew, interval: d.interval, name: name, label: "",
                           numSteps: 1001, parameterIndex: 0 });
                fire(id, { eventType: "valueChanged", value: d.value });
              }, 0);
            } else if (payload.eventType === "valueChanged") {
              d.value = payload.value;
              fire(id, { eventType: "valueChanged", value: payload.value });
            }
            // sliderDragStarted / sliderDragEnded 在 mock 里忽略
          } else if (isToggle) {
            if (payload.eventType === "requestInitialUpdate") {
              setTimeout(function () {
                fire(id, { eventType: "propertiesChanged", name: name, parameterIndex: 0 });
                fire(id, { eventType: "valueChanged", value: toggleDefs[name] });
              }, 0);
            } else if (payload.eventType === "valueChanged") {
              toggleDefs[name] = !!payload.value;
              fire(id, { eventType: "valueChanged", value: toggleDefs[name] });
            }
          }
        },
        emitByBackend: function (id, json) {
          fire(id, typeof json === "string" ? JSON.parse(json) : json);
        }
      }
    };
  }

  var HAS_BACKEND = !!(window.__JUCE__ && window.__JUCE__.backend
                       && typeof window.__JUCE__.backend.emitEvent === "function");

  if (!HAS_BACKEND) installMockBackend();

  var sliderStates = {};
  var toggleStates = {};

  if (typeof window.__JUCE__.getSliderState !== "function") {
    window.__JUCE__.getSliderState = function (name) {
      if (!sliderStates[name]) sliderStates[name] = new SliderState(name);
      return sliderStates[name];
    };
  }
  if (typeof window.__JUCE__.getToggleState !== "function") {
    window.__JUCE__.getToggleState = function (name) {
      if (!toggleStates[name]) toggleStates[name] = new ToggleState(name);
      return toggleStates[name];
    };
  }

  function slider(name) {
    try {
      return window.__JUCE__.getSliderState(name);
    } catch (e) {
      showError("getSliderState(" + name + "): " + e);
      return null;
    }
  }

  function toggle(name) {
    try {
      return window.__JUCE__.getToggleState(name);
    } catch (e) {
      showError("getToggleState(" + name + "): " + e);
      return null;
    }
  }

  /* ---------------- 2. 状态（默认值 = 插件契约默认值） ---------------- */
  var state = {
    input: 0.2000, output: 0.6259, attack: 0.5000, release: 0.6660,
    ratio: 0,   // 0=4:1 1=8:1 2=12:1 3=20:1 4=ALL
    meter: 0,   // 0=GR 1=+8 2=+4 3=OFF
    gr: 0.0, level: 0.0, power: true, bypass: false
  };

  var DEBUG = true;   // 临时：实时显示 INPUT/OUTPUT 增益，核对完删掉

  var cppStatus = HAS_BACKEND ? "wait" : "mock";

  /* ---------------- 3. 资产 / 几何常量 ---------------- */
  var BIG   = { body: A + "knob_body_big.png",   pointer: A + "knob_pointer.png", sizeCss: 96, pivotCss: 48 };
  var SMALL = { body: A + "knob_body_small.png", dot: A + "knob_dot.png",        sizeCss: 60, pivotCss: 30 };
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

  var METER_ROWS = ["meter_gr", "meter_p8", "meter_p4", "meter_off"];
  var METER_KEYS = ["gr", "p8", "p4", "off"];
  var ratioToggles = [false, false, false, true];   // 默认 4:1

  var ASSET_GEO = {
    "btn_ratio_up.png":   { w: 32, h: 40, bx: 7.5, by: 5.5 },
    "btn_ratio_down.png": { w: 32, h: 40, bx: 7.5, by: 5.5 },
    "meter_col_up.png":   { w: 50, h: 150, bx: 15.5, by: 14.0 },
    "meter_btn_up.png":   { w: 32, h: 40, bx: 7.0, by: 5.5 }
  };

  var CHOICE_COUNT = { ratio: 5, meter: 4 };

  /* ---------------- 4. 工具 ---------------- */
  function el(t, a) {
    var n = document.createElement(t);
    if (a) for (var k in a) n.setAttribute(k, a[k]);
    return n;
  }
  function sv(t, a) {
    var n = document.createElementNS(SVGNS, t);
    if (a) for (var k in a) n.setAttribute(k, a[k]);
    return n;
  }
  function find(id) {
    if (!CFG || !CFG.components) return null;
    for (var i = 0; i < CFG.components.length; i++)
      if (CFG.components[i].id === id) return CFG.components[i];
    return null;
  }
  function rad(d) { return d * Math.PI / 180; }
  function pt(cx, cy, r, deg) { return [cx + r * Math.cos(rad(deg)), cy + r * Math.sin(rad(deg))]; }
  function clamp(v, lo, hi) { return v < lo ? lo : (v > hi ? hi : v); }
  function idxToNorm(i, count) { return count > 1 ? i / (count - 1) : 0; }

  /* ---------------- 5. 刻度层（SVG） ---------------- */
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

    // 红区不画：正数区域用红色刻度 + 红色刻度弧表示

    var labs = v.labels, i, a, b;
    // 刻度弧：0 之前墨色，正数段红色
    var REDC = "#c53a28";
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
    /* 红区：同一半径上把弧画粗，接在 0 之后（TOiRE-2A 的做法） */
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

    // 百分比刻度：均分在首尾刻度之间，保证数字落在刻度正下方
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

    // VU 字样
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

  /* ---------------- 6. 旋钮（静态本体 + 旋转指示针） ---------------- */
  function buildKnobs() {
    var host = document.getElementById("knobs");
    KNOBS.forEach(function (k) {
      var c = find(k.id);
      var px = c.pivot_px[0], py = c.pivot_px[1];
      var isBig = (k.kind === "big");
      var bodySize = isBig ? BIG.sizeCss : SMALL.sizeCss;
      var half = bodySize / 2;

      // ⚠️ wrap 必须有真实尺寸：web 版是 0x0 + 子元素 pointer-events:none = 拖不到
      var wrap = el("div", { "class": "knob-wrap", "data-id": k.id });
      wrap.style.left = (px - half) + "px";
      wrap.style.top = (py - half) + "px";
      wrap.style.width = bodySize + "px";
      wrap.style.height = bodySize + "px";

      // 本体
      var bd = el("img", { "class": "knob-body", src: isBig ? BIG.body : SMALL.body });
      bd.style.width = bodySize + "px";
      bd.style.height = bodySize + "px";
      bd.style.left = "0px";
      bd.style.top = "0px";
      wrap.appendChild(bd);
      (function (key, node, size) {
        var img = new Image();
        img.onload = function () {
          node.style.backgroundImage = "url(" + img.src + ")";
          node.src = "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";   // 1px 透明，避免 img 自带图叠在上面
          node.style.backgroundSize = size + "px " + (size * 61) + "px";
          node.style.backgroundRepeat = "no-repeat";
          BODY_STRIPS[key] = { el: node, size: size, frames: 61 };
          render();
        };
        img.src = (window.__ASSET_BASE__ || (typeof A !== "undefined" ? A : "../renders/")) +
                  (size === BIG.sizeCss ? "knob_body_big_61f.png" : "knob_body_small_61f.png");
      })(k.key, bd, bodySize);

      // 指示针（绕旋钮中心转）
      var ind = isBig ? null : el("img", { "class": "knob-ind", src: SMALL.dot });
      if (ind) {
      var sz = isBig ? BIG.sizeCss : DOT.sizeCss;
      var pv = isBig ? BIG.pivotCss : DOT.pivotCss;
      ind.style.width = sz + "px";
      ind.style.height = sz + "px";
      ind.style.left = (half - pv) + "px";
      ind.style.top = (half - pv) + "px";
      ind.style.transformOrigin = pv + "px " + pv + "px";
      wrap.appendChild(ind);
      }

      host.appendChild(wrap);
      bindKnobDrag(wrap, k.key);
    });
  }

  /* 旋钮拖动：**竖直 + 水平两个方向都算数**
       向右 (dx>0)  = 增大
       向上 (dy>0)  = 增大        ← dy 用 "上一帧 clientY - 这一帧 clientY" 得到
       向左 / 向下  = 减小
     参数变大 → 指针顺时针 → 刻度朝 0 dB（不衰减）→ 声音更响。

     灵敏度：两轴位移相加后除以 TRAVEL_PX。
       纯竖直 / 纯水平拖满 300 px 走完 0..1；
       45° 斜拖时两轴各 150 px 走完 —— 两轴是"相加"的，但不会变成两倍速。
     旧版只有 dy、且 180 px 走完，竖直手感更灵敏但也更容易过冲；
     这里兼顾"水平也要能调"和"别太跳"。 */
  var KNOB_TRAVEL_PX = 300;

  function bindKnobDrag(node, key) {
    var dragging = false, lastX = 0, lastY = 0;

    function endDrag() {
      if (!dragging) return;
      dragging = false;
      var st = slider(key);
      if (st) st.sliderDragEnded();
    }

    node.addEventListener("mousedown", function (e) {
      if (e.button !== 0) return;                 // 只响应左键
      dragging = true;
      lastX = e.clientX;
      lastY = e.clientY;
      e.preventDefault();
      var st = slider(key);
      if (st) st.sliderDragStarted();
    });

    window.addEventListener("mousemove", function (e) {
      if (!dragging) return;

      // 在窗口外松开鼠标时收不到 mouseup —— 用 buttons 兜底，避免"粘住"
      if (e.buttons === 0) { endDrag(); return; }

      var dx = e.clientX - lastX;     // 向右为正
      var dy = lastY - e.clientY;     // 向上为正
      lastX = e.clientX;
      lastY = e.clientY;
      if (dx === 0 && dy === 0) return;

      state[key] = clamp(state[key] + (dx + dy) / KNOB_TRAVEL_PX, 0, 1);
      render();
      pushSlider(key, state[key]);
    });

    window.addEventListener("mouseup", function () { endDrag(); });

    node.addEventListener("wheel", function (e) {
      e.preventDefault();
      // 滚轮向上 = 增大（与"向上拖"同向）
      var dir = e.deltaY > 0 ? 1 : (e.deltaY < 0 ? -1 : 0);
      state[key] = clamp(state[key] - dir * 0.02, 0, 1);
      render();
      pushSlider(key, state[key]);
    }, { passive: false });
  }

  /* ---------------- 7. 按键 ---------------- */
  function placeSprite(host, assetName, visualBox, cls) {
    var g = ASSET_GEO[assetName];
    var d = el("div", { "class": "btn " + (cls || ""), "data-asset": assetName });
    d.style.left = (visualBox[0] - g.bx) + "px";
    d.style.top = (visualBox[1] - g.by) + "px";
    d.style.width = g.w + "px";
    d.style.height = g.h + "px";
    d.style.backgroundImage = "url(" + A + assetName + ")";
    host.appendChild(d);
    return d;
  }

  function buildButtons() {
    var host = document.getElementById("buttons");

    RATIO_ROWS.forEach(function (row, idx) {
      var c = find(row.id);
      if (!c) return;
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

    METER_ROWS.forEach(function (id, idx) {
      var c = find(id);
      if (!c) return;
      var d = placeSprite(host, "meter_btn_up.png", c.visual_box || c.box, "meter-btn");
      d.setAttribute("data-row", idx);
      d.addEventListener("click", function () {
        state.meter = idx;
        render();
        pushChoice("meter", idx);
      });
    });
  }

  /* ---------------- 8. 指针 ---------------- */
    /* big-knob pointer: white line over the disc only (r 24..40.5) */
  var POINTERS = [];
  var BODY_STRIPS = {};   // key -> {el, size, frames}（有旋转序列时替换静态 body）
  function buildKnobPointers() {
    var o = document.getElementById("overlay");
    [["knob_input", "input"], ["knob_output", "output"]].forEach(function (pair) {
      var c = find(pair[0]);
      var edge = sv("line", { stroke: "#111", "stroke-width": 2.5, "stroke-linecap": "round", opacity: .85 });
      var core = sv("line", { stroke: "#ffffff", "stroke-width": 1.75, "stroke-linecap": "round" });
      o.appendChild(edge); o.appendChild(core);
      POINTERS.push({ key: pair[1], id: pair[0], cx: c.pivot_px[0], cy: c.pivot_px[1], edge: edge, core: core });
    });
  }
  function renderPointers() {
    for (var i = 0; i < POINTERS.length; i++) {
      var p = POINTERS[i];
      /* ⚠️ 必须带 kind —— 只传 {id} 时 knobTheta 会退到 components.json 的反向角度上，
         于是指针和声音反着走（INPUT/OUTPUT 大旋钮的老 bug）。 */
      var th = knobTheta({ id: p.id, kind: "big", key: p.key }, state[p.key]);
      var rad = (th - 90) * Math.PI / 180.0;
      var r0 = 24.0, r1 = 40.5;
      var x1 = p.cx + r0 * Math.cos(rad), y1 = p.cy + r0 * Math.sin(rad);
      var x2 = p.cx + r1 * Math.cos(rad), y2 = p.cy + r1 * Math.sin(rad);
      [p.edge, p.core].forEach(function (ln) {
        ln.setAttribute("x1", x1.toFixed(2)); ln.setAttribute("y1", y1.toFixed(2));
        ln.setAttribute("x2", x2.toFixed(2)); ln.setAttribute("y2", y2.toFixed(2));
      });
    }
  }

function needleTheta(t) {
    var c = find("meter_needle_single");
    return c.angle_min_deg + t * (c.angle_max_deg - c.angle_min_deg);
  }


  /* 大旋钮刻度（顺时针递增展开）：0 dB 在 519.9°（=159.9°，右侧），每 −6 dB 增加约 40° 顺时针
     INPUT/OUTPUT 刻度数字是**衰减量 dB**：0 dB 在最右、48 之后是 ∞
     （角度表见下方 BIG_KNOB_SCALE；这里只留实测增益表给 HUD 显示用） */
  var BIG_GAIN = {
    input:  [-41.95, -40.42, -38.89, -36.29, -33.68, -29.81, -25.93, -23.36, -20.79, -17.09, -13.39, -11.01, -8.62, -7.34, -6.05, -3.65, -1.25, -0.66, -0.07, -0.04, 0.00],
    output: [-99.99, -93.55, -76.43, -62.35, -55.02, -47.99, -41.53, -35.87, -30.78, -26.12, -21.81, -17.85, -14.48, -11.43, -8.60, -6.06, -4.01, -2.32, -0.98, -0.12, 0.00]
  };
  function bigKnobDb(key, v) {
    var t = BIG_GAIN[key] || BIG_GAIN.input;
    var x = Math.max(0, Math.min(1, v)) * 20;
    var i = Math.min(19, Math.floor(x));
    var f = x - i;
    return t[i] + (t[i + 1] - t[i]) * f;
  }
  /* 大旋钮刻度（照 renders/components.json → markings.input_scale 的实际印字）：
       θ_cw:  −∞=198.5°  48=239.0°  36=279.4°  30=318.9°  24=360.5°
              18=400.5°  12=440.0°  6=479.5°   0=519.9°（=159.9°+360）
       即：**归一化值 v 增大 = θ_cw 增大 = 指针顺时针 = 朝 0 dB = 更响**。

     ⚠️ 不要用 components.json 里 knob_input/output 的 angle_min_deg/angle_max_deg：
        那一对（159.9 → −161.52）是"v↑ → θ_cw↓"的反向约定（angle_min 是 0 dB、
        angle_max 是 −∞），直接用会把指针转反 —— 这正是"指针和声音相反"的根因。
     刻度数字是**标称衰减量**，不是实测增益；指针只按旋钮位置线性走，
     与参考插件一致（指针落在哪个数字上 = 面板印字，不查 dB 表）。 */
  var BIG_KNOB_SCALE = { start: 198.5, sweep: 321.4 };   // −∞ → 0 dB（顺时针）

  function bigKnobTheta(key, v) {
    return BIG_KNOB_SCALE.start + clamp(v, 0, 1) * BIG_KNOB_SCALE.sweep;
  }

  /* 大旋钮判定：id / key / param / kind 任一命中即可。
     ⚠️ renderPointers() 传进来的对象可能只有 id，别只认 kind === "big"（曾经漏过）。 */
  function isBigKnob(k) {
    if (!k) return false;
    if (k.kind === "big") return true;
    if (k.id === "knob_input" || k.id === "knob_output") return true;
    if (k.key === "input" || k.key === "output") return true;
    return k.param === "input" || k.param === "output";
  }

  function knobTheta(k, v) {
    if (isBigKnob(k)) return bigKnobTheta(k.key || k.param || "input", v);
    var c = find(k.id);
    return c.angle_min_deg + v * (c.angle_max_deg - c.angle_min_deg);
  }

  function buildNeedle() {
    var win = document.getElementById("vu-window");
    var c = find("meter_needle_single");
    var needle = find("meter_needle");
    if (!win || !c || !needle) return;

    var px = c.pivot_px[0], py = c.pivot_px[1];
    var box = needle.box;   // 裁剪窗口 651,43,130,66

    [["needle-shadow", A + "needle_shadow.png"],
     ["needle-blade",  A + "needle_blade.png"]].forEach(function (pair) {
      var im = el("img", { id: pair[0], src: pair[1] });
      im.style.width = NEEDLE.sizeCss + "px";
      im.style.height = NEEDLE.sizeCss + "px";
      im.style.left = (px - NEEDLE.pivotCss - box[0]) + "px";
      im.style.top = (py - NEEDLE.pivotCss - box[1]) + "px";
      im.style.transformOrigin = NEEDLE.pivotCss + "px " + NEEDLE.pivotCss + "px";
      win.appendChild(im);
    });
  }

  /* ---------------- 9. POWER / BYPASS + HUD ---------------- */
  function buildToggles() {
    var p = document.getElementById("tog-power");
    var b = document.getElementById("tog-bypass");

    if (p) p.addEventListener("click", function () {
      state.power = !state.power;
      render();
      var ts = toggle("power");
      if (ts) ts.setValue(state.power);
    });

    if (b) b.addEventListener("click", function () {
      state.bypass = !state.bypass;
      render();
      var ts = toggle("bypass");
      if (ts) ts.setValue(state.bypass);
    });

    var hud = document.getElementById("hud");
    if (hud && DEBUG) hud.style.display = "block";
  }

  /* ---------------- 10. render ---------------- */
  var VU_POINTS = null;
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
  var NEEDLE_PHYS = { cur: null, vel: 0, target: 0, raf: 0, mode: -1, stop: false };

  /* 表针可调参数（调试面板里实时改，localStorage 记忆）
       omega : 上升速度（rad/s）—— 越大越快
       zeta  : 阻尼比 —— 越小过冲越多、越"晃"
       atkMs : 电平表攻击时间（毫秒）
       relMs : 电平表释放时间（毫秒）
       grRel : GR 档额外回落平滑（毫秒，0=不加，交给弹簧本身）
     默认值：ω=13.1026 rad/s、ζ=0.80 → 300 ms 到 99%、过冲 1.52%（VU 规范）
     ⚠️ 内部子步必须 ≥120 Hz，否则过冲会被离散化吃掉（我们是 240 Hz）*/
  var NEEDLE_CFG = { omega: 13.1026, zeta: 0.80, atkMs: 5, relMs: 300, grRelMs: 0 };
  try {
    var _saved = JSON.parse(window.localStorage.getItem("t76needle") || "{}");
    for (var _k in NEEDLE_CFG) if (typeof _saved[_k] === "number" && isFinite(_saved[_k])) NEEDLE_CFG[_k] = _saved[_k];
  } catch (e) {}
  function saveNeedleCfg() {
    try { window.localStorage.setItem("t76needle", JSON.stringify(NEEDLE_CFG)); } catch (e) {}
  }

  function needleApply(deg) {
    var blade = document.getElementById("needle-blade");
    var shadow = document.getElementById("needle-shadow");
    if (blade) blade.style.transform = "rotate(" + deg.toFixed(2) + "deg)";
    if (shadow) shadow.style.transform = "rotate(" + deg.toFixed(2) + "deg)";
  }

  function needleSetTarget(thetaDeg, mode) {
    var p = NEEDLE_PHYS;
    if (p.cur === null) { p.cur = thetaDeg; p.vel = 0; needleApply(p.cur); }
    p.mode = mode;
    p.target = thetaDeg;
    if (!p.raf) { p.stop = false; needleLoop(); }
  }

  function needleLoop() {
    var p = NEEDLE_PHYS;
    p.raf = 1;
    var last = 0;
    var w = NEEDLE_CFG.omega;
    var z = NEEDLE_CFG.zeta;
    var h = 1.0 / 240.0;          // 固定子步长，避免大 dt 发散

    function step(ts) {
      if (p.stop) { p.raf = 0; p.stop = false; return; }
      if (!last) { last = ts; requestAnimationFrame(step); return; }
      var dt = Math.min(0.05, (ts - last) / 1000);
      last = ts;

      var n = Math.max(1, Math.round(dt / h));
      var sub = dt / n;
      for (var i = 0; i < n; i++) {
        var a = w * w * (p.target - p.cur) - 2 * z * w * p.vel;
        p.vel += a * sub;
        p.cur += p.vel * sub;
      }
      needleApply(p.cur);

      var settled = Math.abs(p.vel) < 0.35 && Math.abs(p.cur - p.target) < 0.06;
      if (settled) {
        p.vel = 0;
        p.cur = p.target;
        needleApply(p.cur);
        p.raf = 0;
        return;
      }
      requestAnimationFrame(step);
    }
    requestAnimationFrame(step);
  }

  /* 紧凑 HUD：实时显示 INPUT/OUTPUT 与 GR（调试用，确认后可删） */
  function renderHud() {
    if (!DEBUG) return;
    var hud = document.getElementById("hud");
    if (!hud) return;
    var inDb = bigKnobDb("input", state.input), outDb = bigKnobDb("output", state.output);
    var inTh = bigKnobTheta("input", state.input) % 360;
    hud.textContent =
      "IN  " + state.input.toFixed(3) + "  " + inDb.toFixed(1) + " dB  " + inTh.toFixed(0) + "°\n" +
      "OUT " + state.output.toFixed(3) + "  " + outDb.toFixed(1) + " dB\n" +
      "GR  " + state.gr.toFixed(1) + " dB\n" +
      "ω " + NEEDLE_CFG.omega.toFixed(1) + "  ζ " + NEEDLE_CFG.zeta.toFixed(2) + "  释放 " + Math.round(NEEDLE_CFG.relMs) + " ms";
  }

  /* VU 表针：GR 档读压缩量（0 在右、-20 在左），+8/+4 档读输出电平，OFF 停最左 */
  function renderMeter() {
    var dbNow = -20, mode = state.meter;
    if (!state.power) {
      dbNow = -20;
    } else if (mode === 0) {
      // GR：直接按 VU 刻度读数 —— 压缩 10 dB 就指在表盘 "-10" 位置
      // （UAD 的 GR 表就是同一块 VU 表盘，指针读的是"负的压缩量"）
      dbNow = -(state.gr || 0);
    } else if (mode === 1 || mode === 2) {
      // +8/+4：输出峰值电平（dBFS）直接读；本插件输出满度 ≈ 0 dBFS
      var outDb = (typeof state.outDb === "number" && isFinite(state.outDb)) ? state.outDb : -100;
      dbNow = outDb + (mode === 1 ? 8 : 4);
    }
    if (dbNow < -20) dbNow = -20;
    if (dbNow > 3) dbNow = 3;
    var t = (vuThetaForDb(dbNow) + 49.13) / 100.29;
    if (t < 0) t = 0; if (t > 1) t = 1;
    needleSetTarget(parseFloat(needleTheta(t).toFixed(2)), mode);
  }

  /* 表针参数调试面板（仅 DEBUG）：ω / ζ / 释放时间，实时生效并记忆 */
  function buildTuner() {
    if (!DEBUG) return;
    if (document.getElementById("tuner")) return;
    var d = el("div", { id: "tuner" });
    var rows = [
      ["omega", "上升 ω", 2, 40, 0.1, "rad/s"],
      ["zeta",  "阻尼 ζ", 0.15, 1.5, 0.01, ""],
      ["relMs", "释放(ms)", 20, 2000, 10, "ms"],
      ["grRelMs", "GR回落", 0, 800, 10, "ms"]
    ];
    rows.forEach(function (r) {
      var wrap = el("div", { "class": "trow" });
      var lab = el("span"); lab.textContent = r[1];
      var inp = el("input", { type: "range", min: r[2], max: r[3], step: r[4] });
      inp.value = NEEDLE_CFG[r[0]];
      var val = el("span"); val.className = "tval";
      val.textContent = (r[0] === "zeta" ? NEEDLE_CFG[r[0]].toFixed(2) : Math.round(NEEDLE_CFG[r[0]])) + r[5];
      inp.addEventListener("input", function () {
        NEEDLE_CFG[r[0]] = parseFloat(inp.value);
        if (r[0] === "grRelMs") { state.smoothGr = undefined; }
        val.textContent = (r[0] === "zeta" ? NEEDLE_CFG[r[0]].toFixed(2) : Math.round(NEEDLE_CFG[r[0]])) + r[5];
        saveNeedleCfg();
        renderHud();
      });
      wrap.appendChild(lab); wrap.appendChild(inp); wrap.appendChild(val);
      d.appendChild(wrap);
    });
    document.body.appendChild(d);
  }

  function render() {
    renderPointers();
    // 旋钮指示针
    KNOBS.forEach(function (k) {
      var wrap = document.querySelector('.knob-wrap[data-id="' + k.id + '"]');
      if (!wrap) return;
      var strip = BODY_STRIPS[k.key];
      if (strip) {
        /* 序列帧只影响滚花/高光相位 —— 贴图里**没有**白线指针（指针是 overlay 里的 SVG 直线）。
           UAD 素材的帧序与语义值相反（frame0 = 0 dB 位、帧号递增 = θ_cw 递减），
           所以统一用 (frames-1) - round(v*(frames-1))，和大旋钮指针的顺时针方向一致。
           实测 61 帧之间只有 <2% 像素有差别，肉眼几乎看不出，方向对就行。 */
        var fr = (strip.frames - 1) - Math.round(state[k.key] * (strip.frames - 1));
        strip.el.style.backgroundPositionY = (-fr * strip.size) + "px";
      }
      var ind = wrap.querySelector(".knob-ind");
      if (!ind) return;
      ind.style.transform = "rotate(" + knobTheta(k, state[k.key]).toFixed(2) + "deg)";
    });

    // RATIO 四个按键
    var btn = document.querySelectorAll(".ratio-btn");
    for (var i = 0; i < btn.length; i++) {
      var down = (state.ratio === 4) ? true : (state.ratio === RATIO_ROWS[i].ratio);
      btn[i].style.backgroundImage = "url(" + A + "btn_ratio_" + RATIO_KEYS[i] + (down ? "_down.png" : "_up.png") + ")";
    }

    // METER 四档
    var mbn = document.querySelectorAll(".meter-btn");
    for (var mi = 0; mi < mbn.length; mi++) {
      var downM = (state.meter === mi);
      mbn[mi].style.backgroundImage = "url(" + A + "meter_btn_" + METER_KEYS[mi] + (downM ? "_down.png" : "_up.png") + ")";
    }

    // 表针
    renderMeter();

    // POWER / BYPASS 指示
    var p = document.getElementById("tog-power");
    var b = document.getElementById("tog-bypass");
    if (p) p.className = state.power ? "tog on" : "tog";
    if (b) b.className = state.bypass ? "tog on" : "tog";

    renderHud();
  }

  /* ---------------- 11. 参数绑定（APVTS <-> 网页） ---------------- */
  function pushSlider(name, normalised) {
    var st = slider(name);
    if (!st) return;
    st.setNormalisedValue(clamp(normalised, 0, 1));
  }

  /* 宿主（APVTS）回读的归一化值：JUCE relay 的 valueChanged 带的是 scaled 值，
     getNormalisedValue() 再按 start/end/skew 换回来。0..1 参数下与 n 完全相同；
     HUD 里对比 n 和 host 就能看出"参数回传有没有被反向/夹住"。 */
  function hostNorm(name) {
    var st = slider(name);
    if (!st) return NaN;
    var v = st.getNormalisedValue();
    return isFinite(v) ? v : NaN;
  }

  function pushChoice(name, index) {
    var count = CHOICE_COUNT[name] || 2;
    pushSlider(name, idxToNorm(clamp(index, 0, count - 1), count));
  }

  function bindSliderParam(name, key, isChoice, count) {
    var st = slider(name);
    if (!st) return;

    // ⚠️ JUCE 的 valueChangedEvent 回调不带参数，要自己从 state 读当前值
    st.valueChangedEvent.addListener(function () {
      if (isChoice) {
        var idx = Math.round(st.getScaledValue());
        if (isFinite(idx)) state[key] = clamp(idx, 0, count - 1);
      } else {
        var v = st.getNormalisedValue();
        if (isFinite(v)) state[key] = clamp(v, 0, 1);
      }
      render();
    });
  }

  function bindToggleParam(name, key) {
    var ts = toggle(name);
    if (!ts) return;
    ts.valueChangedEvent.addListener(function () {
      state[key] = !!ts.getValue();
      render();
    });
  }

  function bindParams() {
    bindSliderParam("input",   "input",   false, 0);
    bindSliderParam("output",  "output",  false, 0);
    bindSliderParam("attack",  "attack",  false, 0);
    bindSliderParam("release", "release", false, 0);
    bindSliderParam("ratio",   "ratio",   true,  CHOICE_COUNT.ratio);
    bindSliderParam("meter",   "meter",   true,  CHOICE_COUNT.meter);
    bindToggleParam("power",  "power");
    bindToggleParam("bypass", "bypass");

    // C++ -> JS 的状态事件（证明桥是通的；不走 evaluateJavascript）
    if (HAS_BACKEND && window.__JUCE__.backend.addEventListener) {
      window.__JUCE__.backend.addEventListener("toire76Status", function (payload) {
        cppStatus = (payload && payload.length) ? ("cpp " + payload[0]) : "cpp ok";
        render();
      });

      // 实时 GR + 输入/输出电平（30 Hz，来自音频线程的原子量）
      window.__JUCE__.backend.addEventListener("toire76Meter", function (payload) {
        var nowT = (window.performance && performance.now) ? performance.now() : Date.now();
        var dts = Math.max(0.004, Math.min(0.2, (nowT - (state._lastT || nowT)) / 1000));
        state._lastT = nowT;

        if (payload && payload.length >= 3) {
          var rawGr  = parseFloat(payload[0]) || 0;
          var rawOut = parseFloat(payload[2]);
          // 电平：攻击快、释放慢（时间可调）
          var aAtk = 1 - Math.exp(-dts / Math.max(0.001, NEEDLE_CFG.atkMs / 1000));
          var aRel = 1 - Math.exp(-dts / Math.max(0.001, NEEDLE_CFG.relMs / 1000));
          if (typeof state.smoothOutDb !== "number" || !isFinite(state.smoothOutDb)) state.smoothOutDb = rawOut;
          state.smoothOutDb += (rawOut - state.smoothOutDb) * (rawOut > state.smoothOutDb ? aAtk : aRel);
          state.outDb = state.smoothOutDb;
          state.inDb  = parseFloat(payload[1]);
          // GR 直接喂给表针弹簧（UAD 的平滑在算法侧，GUI 零平滑）
          if (NEEDLE_CFG.grRelMs > 20) {
            var aGrRel = 1 - Math.exp(-dts / (NEEDLE_CFG.grRelMs / 1000));
            if (typeof state.smoothGr !== "number" || !isFinite(state.smoothGr)) state.smoothGr = rawGr;
            state.smoothGr += (rawGr - state.smoothGr) * (rawGr > state.smoothGr ? 1 : aGrRel);
            state.gr = state.smoothGr;
          } else {
            state.gr = rawGr;
          }
        } else if (payload && payload.length) {
          state.gr = parseFloat(payload[0]) || 0;
        }
        renderHud();
        renderMeter();
      });
    }
  }

  /* ---------------- 12. 等比缩放到窗口 ---------------- */
  function fit() {
    var z = Math.min(window.innerWidth / 950, window.innerHeight / 175);
    if (!(z > 0) || !isFinite(z)) z = 1;
    document.documentElement.style.setProperty("--zoom", z);
  }

  /* ---------------- 13. boot ---------------- */
    // 开发用：?meter=0..3&gr=0..1&level=0..1 覆盖初始状态
  (function () {
    var q = location.search || "";
    function num(k) { var m = q.match(new RegExp("[?&]" + k + "=([0-9.]+)")); return m ? parseFloat(m[1]) : null; }
    var mv = num("meter"); if (mv !== null) state.meter = Math.max(0, Math.min(3, Math.round(mv)));
    var gv = num("gr"); if (gv !== null) state.gr = Math.max(0, Math.min(1, gv));
    var lv = num("level"); if (lv !== null) state.level = Math.max(0, Math.min(1, lv));
    var rv = num("ratio"); if (rv !== null) state.ratio = Math.max(0, Math.min(4, Math.round(rv)));
  })();

function boot() {
    buildOverlay();
    buildKnobPointers();
    buildKnobs();
    buildButtons();
    buildNeedle();
    buildToggles();
  buildTuner();
    bindParams();
    render();
    fit();
    window.addEventListener("resize", fit);

    window.TOIRE76 = {
      state: state,
      render: render,
      pushSlider: pushSlider,
      pushChoice: pushChoice,
      knobTheta: knobTheta,       // 供调试/截图核对：{id,kind,key} + n → theta_cw
      bigKnobTheta: bigKnobTheta,
      hasBackend: HAS_BACKEND
    };
  }

  if (!CFG || !CFG.components) {
    showError("ui-config.js 未加载：window.TOIRE76_CFG 缺失（WebUI/index.html 应先加载 ui-config.js）");
    return;
  }

  try {
    boot();
  } catch (e) {
    showError("FATAL: " + e.message + "\n" + (e.stack || ""));
  }
})();
