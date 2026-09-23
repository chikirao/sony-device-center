/* Sony Device Center site behaviour. No framework, no build step: this
   file, dotglyphs.js (the app's glyph table) and halftone.js (the product
   as a dot grid) are all there is. */
(function () {
  "use strict";

  var REPO = "chikirao/sony-device-center";
  var reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  var darkScheme = window.matchMedia("(prefers-color-scheme: dark)");

  function ink() { return getComputedStyle(document.documentElement).getPropertyValue("--ink").trim() || "#111"; }

  /* ---------- Brand mark: twenty dots tracing an S (from assets/mark.svg) ---------- */
  var MARK = [[0.815, 0.063], [0.447, 0.065], [0.630, 0.065], [0.265, 0.067], [0.091, 0.209], [0.265, 0.209],
              [0.091, 0.361], [0.265, 0.361], [0.783, 0.502], [0.257, 0.505], [0.435, 0.505], [0.610, 0.505],
              [0.759, 0.646], [0.909, 0.646], [0.757, 0.793], [0.909, 0.793], [0.255, 0.935], [0.428, 0.935],
              [0.601, 0.935], [0.760, 0.937]];
  var mark = document.getElementById("brand-mark");
  if (mark) mark.innerHTML = MARK.map(function (p) { return '<circle cx="' + p[0] + '" cy="' + p[1] + '" r="0.0634"/>'; }).join("");

  /* ---------- Dot-matrix text, the app's DotText on SVG ---------- */
  // Same grid (5x7, pitch 1.55 dot), same left-to-right sweep with a stable
  // per-dot jitter, so a word lands here the way it lands in the app.
  function renderDots(el, text) {
    var G = window.DotGlyphs;
    var dot = parseFloat(el.getAttribute("data-dot")) || 4;
    var pitch = dot * 1.55;
    var chars = Array.from(G ? G.normalize(text) : "");
    var circles = [], x = 0;
    for (var i = 0; i < chars.length; ++i) {
      var glyph = G && G.table[chars[i]];
      if (!glyph) { el.innerHTML = '<span class="dots__fallback">' + text + "</span>"; return; }
      var w = glyph[0].length;
      for (var r = 0; r < 7; ++r)
        for (var c = 0; c < w; ++c)
          if (glyph[r].charAt(c) === "#") {
            var jitter = ((x + c) * 37 + r * 91 + i * 17) % 100 / 100;
            var delay = Math.round((x + c) * 11 + jitter * 120);
            circles.push('<circle cx="' + ((x + c) * pitch + dot / 2).toFixed(2) + '" cy="' + (r * pitch + dot / 2).toFixed(2) +
                         '" r="' + (dot / 2).toFixed(2) + '" style="--d:' + delay + 'ms"/>');
          }
      x += w + 1;
    }
    var width = Math.max(0, (x - 1) * pitch - (pitch - dot));
    var height = 7 * pitch - (pitch - dot);
    el.style.width = "min(100%, " + width.toFixed(1) + "px)";
    el.innerHTML = '<svg viewBox="0 0 ' + width.toFixed(2) + " " + height.toFixed(2) + '" aria-hidden="true">' + circles.join("") + "</svg>";
  }
  function light(el) {
    el.classList.remove("is-lit");
    void el.offsetWidth; // restart the sweep
    el.classList.add("is-lit");
  }

  /* ---------- Reveal on scroll ---------- */
  var revealer = "IntersectionObserver" in window ? new IntersectionObserver(function (entries) {
    entries.forEach(function (e) {
      if (!e.isIntersecting) return;
      e.target.classList.add("is-in");
      if (e.target.classList.contains("dots")) light(e.target);
      revealer.unobserve(e.target);
    });
  }, { rootMargin: "0px 0px -8% 0px", threshold: 0.12 }) : null;
  document.querySelectorAll(".reveal").forEach(function (el) { revealer ? revealer.observe(el) : el.classList.add("is-in"); });

  // Device names: each lights up as it scrolls in, a beat after the one before.
  document.querySelectorAll(".models .dots").forEach(function (el, index) {
    var name = el.textContent.trim();
    el.setAttribute("role", "img");
    el.setAttribute("aria-label", name);
    renderDots(el, name);
    el.querySelectorAll("circle").forEach(function (c) {
      c.style.setProperty("--d", (parseFloat(c.style.getPropertyValue("--d")) + (index % 3) * 140) + "ms");
    });
    revealer ? revealer.observe(el) : el.classList.add("is-lit");
  });

  /* ---------- Sticky nav hairline ---------- */
  // A sentinel at the very top: once it leaves the viewport, the page has
  // scrolled under the nav.
  var nav = document.querySelector(".nav");
  if (nav && "IntersectionObserver" in window) {
    var sentinel = document.createElement("div");
    sentinel.setAttribute("aria-hidden", "true");
    sentinel.style.cssText = "position:absolute;top:0;left:0;width:1px;height:8px;pointer-events:none";
    document.body.prepend(sentinel);
    new IntersectionObserver(function (entries) { nav.classList.toggle("is-scrolled", !entries[0].isIntersecting); }).observe(sentinel);
  }

  /* ---------- Back to top ---------- */
  // The wordmark always goes to the top. A plain "#top" does nothing once
  // the address already ends in it, so scroll by hand and drop the hash.
  var brand = document.querySelector(".brand");
  if (brand) brand.addEventListener("click", function (e) {
    e.preventDefault();
    var still = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
    window.scrollTo({ top: 0, behavior: still ? "auto" : "smooth" });
    if (location.hash) history.replaceState(null, "", location.pathname + location.search);
  });

  /* ---------- Halftone headphones ---------- */
  // The product as a grid of ink dots. The sound mode changes how the grid
  // behaves: cancelling holds it dense and still, ambient lets a slow wave
  // run through it from the middle, off leaves it light and quiet. The
  // pointer blows the dots apart like dust: they swell and drift away from
  // it, faster the faster it moves, and spring back to their place once it
  // leaves. Frames are drawn only while something moves and the hero is on
  // screen.
  var canvas = document.getElementById("halftone");
  var H = window.HALFTONE;
  var art = null;
  if (canvas && H && canvas.getContext) {
    art = (function () {
      var ctx = canvas.getContext("2d");
      // Per-dot randomness for the scatter. A proper hash: the linear one
      // the intro uses changes smoothly down a column, which made whole
      // vertical runs of dots fly off together.
      function hash(a, b) { var n = Math.sin(a * 127.1 + b * 311.7) * 43758.5453; return n - Math.floor(n); }
      var cells = [];
      for (var y = 0; y < H.rows; ++y)
        for (var x = 0; x < H.columns; ++x) {
          var v = H.cells.charCodeAt(y * H.columns + x) - 48;
          if (v > 0) cells.push({ x: x, y: y, v: Math.sqrt(v / 9), j: ((x * 37 + y * 91) % 100) / 100,
                                  dx: 0, dy: 0, vx: 0, vy: 0, g: 0, h: hash(x, y), k: hash(y + 101, x + 37) });
        }
      var size = { w: 0, h: 0, cell: 0, ox: 0, oy: 0, dpr: 1 };
      var weights = { cancelling: 1, ambient: 0, off: 0 };
      var target = "cancelling";
      var introStart = -1, introLength = 0;
      var pointer = null, speed = 0, lastFrame = 0;
      var visible = true, frame = 0, color = ink();

      function resize() {
        var rect = canvas.getBoundingClientRect();
        size.dpr = Math.min(window.devicePixelRatio || 1, 2);
        size.w = rect.width; size.h = rect.height;
        canvas.width = Math.round(rect.width * size.dpr);
        canvas.height = Math.round(rect.height * size.dpr);
        size.cell = Math.min(size.w / H.columns, size.h / H.rows);
        size.ox = (size.w - size.cell * H.columns) / 2;
        size.oy = (size.h - size.cell * H.rows) / 2;
        request();
      }

      function draw(now) {
        frame = 0;
        var moving = false;
        var still = reduceMotion.matches;
        // Ease the mode weights towards the chosen mode.
        for (var m in weights) {
          var goal = m === target ? 1 : 0;
          var next = still ? goal : weights[m] + (goal - weights[m]) * 0.12;
          if (Math.abs(next - goal) < 0.002) next = goal; else moving = true;
          weights[m] = next;
        }
        var t = still ? 0 : now;
        var dt = lastFrame ? Math.min(3, (now - lastFrame) / 16.7) : 1;
        lastFrame = now;
        // The pointer's push fades with its speed, so a resting cursor holds
        // a calm hole and a fast one kicks up more dust.
        speed *= Math.pow(0.9, dt);
        var intro = 1;
        if (introStart >= 0 && !still) {
          intro = (now - introStart) / introLength;
          if (intro < 1) moving = true; else introStart = -1;
        }
        var cell = size.cell, half = cell / 2;
        var cx = size.ox + cell * H.columns / 2, cy = size.oy + cell * H.rows * 0.42;
        var reach = cell * 20;
        var push = cell * (0.08 + Math.min(0.14, speed * 0.008));
        ctx.setTransform(size.dpr, 0, 0, size.dpr, 0, 0);
        ctx.clearRect(0, 0, size.w, size.h);
        ctx.fillStyle = color;
        ctx.beginPath();
        for (var i = 0; i < cells.length; ++i) {
          var c = cells[i];
          var hx = size.ox + c.x * cell + half, hy = size.oy + c.y * cell + half;
          var r = half * c.v * 0.94;
          // Mode shaping.
          var dist = Math.hypot(hx - cx, hy - cy) / cell;
          var wave = 0.5 + 0.5 * Math.sin(dist * 0.5 - t * 0.0022);
          r *= weights.cancelling * 1.0 + weights.ambient * (0.66 + 0.4 * wave) + weights.off * 0.62;
          // Intro sweep, column by column like the dot text.
          if (intro < 1) {
            var local = (intro * (H.columns + 30) - c.x - c.j * 24) / 10;
            if (local <= 0) continue;
            if (local < 1) r *= 1 - Math.pow(1 - local, 3);
          }
          // Scatter: pushed out from the pointer at a slightly skewed angle
          // per dot (so it reads as dust, not a lens), pulled home by a
          // damped spring.
          var grow = 0;
          if (pointer && !still) {
            var ax = hx - pointer.x, ay = hy - pointer.y;
            var d = Math.hypot(ax, ay);
            if (d < reach) {
              var f = Math.pow(1 - d / reach, 2);
              var angle = Math.atan2(ay, ax) + (c.h - 0.5) * 0.8;
              var strength = push * f * (0.7 + c.k * 0.6) * dt;
              c.vx += Math.cos(angle) * strength;
              c.vy += Math.sin(angle) * strength;
              grow = f;
            }
          }
          if (!still) {
            c.vx += -c.dx * 0.07 * dt; c.vy += -c.dy * 0.07 * dt;
            var damp = Math.pow(0.84, dt);
            c.vx *= damp; c.vy *= damp;
            c.dx += c.vx * dt; c.dy += c.vy * dt;
            c.g += (grow - c.g) * Math.min(1, 0.18 * dt);
            if (Math.abs(c.dx) + Math.abs(c.dy) > 0.02 || Math.abs(c.vx) + Math.abs(c.vy) > 0.02 || c.g > 0.005) moving = true;
            else { c.dx = c.dy = c.vx = c.vy = 0; c.g = 0; }
          } else { c.dx = c.dy = c.vx = c.vy = 0; c.g = 0; }
          var px = hx + c.dx, py = hy + c.dy;
          r *= 1 + 0.6 * c.g;
          if (r < 0.25) continue;
          ctx.moveTo(px + r, py);
          ctx.arc(px, py, r, 0, Math.PI * 2);
        }
        ctx.fill();
        if (weights.ambient > 0.01 && !still) moving = true;
        if (moving && visible) request();
      }
      function request() { if (!frame) frame = requestAnimationFrame(draw); }

      canvas.addEventListener("pointermove", function (e) {
        var rect = canvas.getBoundingClientRect();
        var next = { x: e.clientX - rect.left, y: e.clientY - rect.top };
        if (pointer) speed = Math.max(speed, Math.hypot(next.x - pointer.x, next.y - pointer.y));
        pointer = next;
        request();
      });
      var release = function () { pointer = null; request(); };
      canvas.addEventListener("pointerleave", release);
      canvas.addEventListener("pointercancel", release);
      canvas.addEventListener("pointerup", function (e) { if (e.pointerType !== "mouse") release(); });
      window.addEventListener("resize", resize);
      darkScheme.addEventListener("change", function () { color = ink(); request(); });
      reduceMotion.addEventListener("change", request);
      if ("IntersectionObserver" in window)
        new IntersectionObserver(function (entries) { visible = entries[0].isIntersecting; if (visible) request(); }).observe(canvas);
      document.addEventListener("visibilitychange", function () { if (!document.hidden) request(); });

      resize();
      return {
        intro: function () { introStart = performance.now(); introLength = 1500; request(); },
        mode: function (m) { target = m; request(); }
      };
    })();
    art.intro();
  }

  /* ---------- Hero ticker and the mode switch ---------- */
  var MODES = [
    { mode: "cancelling", text: "Noise cancelling" },
    { mode: "ambient", text: "Ambient 12" },
    { mode: "off", text: "Off" }
  ];
  var ticker = document.getElementById("ticker");
  var buttons = document.querySelectorAll(".modes button");
  var current = 0, auto = true, timer = 0;
  function show(index) {
    current = index;
    var m = MODES[index];
    if (ticker) {
      ticker.setAttribute("aria-label", m.text);
      renderDots(ticker, m.text);
      reduceMotion.matches ? ticker.classList.add("is-lit") : light(ticker);
    }
    buttons.forEach(function (b) { b.setAttribute("aria-pressed", String(b.getAttribute("data-mode") === m.mode)); });
    if (art) art.mode(m.mode);
  }
  function schedule() {
    clearTimeout(timer);
    if (!auto || reduceMotion.matches || document.hidden) return;
    timer = setTimeout(function () { show((current + 1) % MODES.length); schedule(); }, 3600);
  }
  buttons.forEach(function (b) {
    b.addEventListener("click", function () {
      auto = false;
      clearTimeout(timer);
      var i = MODES.findIndex(function (m) { return m.mode === b.getAttribute("data-mode"); });
      if (i >= 0) show(i);
    });
  });
  document.addEventListener("visibilitychange", schedule);
  show(0);
  schedule();

  /* ---------- Features: the stage follows the text ---------- */
  var stage = document.querySelector(".shot--stage");
  var features = document.querySelectorAll(".feature");
  if (stage && "IntersectionObserver" in window) {
    var featureWatch = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (!e.isIntersecting) return;
        var key = e.target.getAttribute("data-shot");
        features.forEach(function (f) { f.classList.toggle("is-active", f === e.target); });
        stage.querySelectorAll("picture").forEach(function (p) { p.classList.toggle("is-on", p.getAttribute("data-shot") === key); });
      });
    }, { rootMargin: "-45% 0px -45% 0px" });
    features.forEach(function (f) { featureWatch.observe(f); });
    if (features[0]) features[0].classList.add("is-active");
  }

  /* ---------- Download: the visitor's system first, real file links ---------- */
  function detectOs() {
    var p = ((navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || navigator.userAgent || "").toLowerCase();
    if (p.indexOf("win") >= 0) return "windows";
    if (p.indexOf("mac") >= 0) return "mac";
    if (p.indexOf("linux") >= 0 || p.indexOf("x11") >= 0) return "linux";
    return "";
  }
  var os = detectOs();
  var OS_LABEL = { windows: "Windows", mac: "macOS", linux: "Linux" };
  var OS_ASSET = { windows: "win64.msi", mac: "macOS.dmg", linux: "Linux.deb" };
  var ctaLabel = document.getElementById("cta-label");
  var cta = document.getElementById("cta-download");
  if (os) {
    if (ctaLabel) ctaLabel.textContent = "Download for " + OS_LABEL[os];
    var row = document.querySelector('.platform[data-os="' + os + '"]');
    if (row) {
      row.classList.add("is-yours");
      var name = row.querySelector(".platform__name");
      if (name) name.insertAdjacentHTML("beforeend", '<span class="platform__yours">Your system</span>');
      row.parentNode.insertBefore(row, row.parentNode.firstChild);
    }
  }
  document.querySelectorAll(".platform").forEach(function (row) { row.classList.add("reveal"); revealer && revealer.observe(row); });

  if (window.fetch) {
    fetch("https://api.github.com/repos/" + REPO + "/releases/latest", { headers: { Accept: "application/vnd.github+json" } })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (release) {
        if (!release || !release.assets) return;
        var version = String(release.tag_name || "").replace(/^v/, "");
        var date = release.published_at ? new Date(release.published_at).toLocaleDateString("en-GB", { day: "numeric", month: "long", year: "numeric" }) : "";
        var line = document.getElementById("release-line");
        if (line && version) line.textContent = "Version " + version + (date ? ", released " + date : "");
        function urlFor(suffix) {
          var a = release.assets.find(function (x) { return x.name.slice(-suffix.length) === suffix; });
          return a ? a.browser_download_url : "";
        }
        document.querySelectorAll("[data-asset]").forEach(function (link) {
          var url = urlFor(link.getAttribute("data-asset"));
          if (url) link.href = url;
        });
        if (os && cta) {
          var url = urlFor(OS_ASSET[os]);
          if (url) cta.href = url;
          var meta = document.getElementById("cta-meta");
          if (meta && version) meta.textContent = "Version " + version + ". Free and open source, MIT license.";
        }
      })
      .catch(function () { /* the links still point at the release page */ });
  }
})();
