/**
 * Raw BLE/HID report explorer: hex / numeric / bit panels with per-byte diff vs previous packet.
 * DOM is created once; updates only patch text/classes when raw bytes change.
 */
(function () {
  const PACKET_FIELD_HINTS = [
    { start: 0x00, len: 4, label: "JC2: packet id / seq (often u32 LE)" },
    {
      start: 0x07,
      len: 1,
      label:
        "JC2: 0x07 的 b7、b6：接入 USB/充电时常为 0；拔下后常为 1（两 bit 置 1）；解码为 chargerBits67 / chargerCableConnected"
    },
    { start: 0x03, len: 3, label: "Current decoder: R Joy-Con buttons (24-bit @3)" },
    { start: 0x04, len: 3, label: "Current decoder: L Joy-Con buttons (24-bit @4)" },
    { start: 0x04, len: 4, label: "JC2: buttons (u32 LE)" },
    { start: 0x08, len: 3, label: "JC2: L stick 12-bit packed" },
    { start: 0x0a, len: 3, label: "Current decoder: L stick @0x0A (10)" },
    { start: 0x0b, len: 3, label: "JC2: R stick (garbage on L-JC)" },
    { start: 0x0d, len: 3, label: "Current decoder: R stick @0x0D (13)" },
    { start: 0x0e, len: 2, label: "JC2: mouse / optical X (s16 LE?)" },
    { start: 0x10, len: 4, label: "JC2: mouse Y + unk / v1 optical XY s16 @0x10" },
    { start: 0x14, len: 2, label: "JC2: mouse unk A (u16 LE @0x14)" },
    {
      start: 0x16,
      len: 2,
      label: "JC2: optical distance u16 LE @0x16；门控：raw ≤ 近距离上限（全 16 位）"
    },
    { start: 0x19, len: 2, label: "JC2: magnetometer X s16 LE @0x19" },
    { start: 0x1b, len: 2, label: "JC2: magnetometer Y s16 LE (@0x1B)" },
    { start: 0x1d, len: 2, label: "JC2: magnetometer Z s16 LE (@0x1D)" },
    {
      start: 0x1f,
      len: 2,
      label: "JC2: 电池电压 u16 LE mV @0x1F（官方额定 3.89V / 500mAh / 1.95Wh）"
    },
    {
      start: 0x22,
      len: 2,
      label:
        "JC2: 电流原始值 u16 LE @0x22（解码为 int16 LE；常见按 /100 ≈ mA；例：1916 → ~19 mA）"
    },
    {
      start: 0x24,
      len: 6,
      label: "JC2: 填充 (~0x24–0x29)；motion @0x2A 起 18B"
    },
    {
      start: 0x2c,
      len: 2,
      label:
        "L-JC: primary temperature-related raw u16 LE (fridge + hot-water validated; monotonic, hotter=larger; degC TBD)"
    },
    {
      start: 0x2e,
      len: 2,
      label:
        "R-JC: primary temperature raw s16 LE (fridge + hot-water validated; colder=more negative); ~degC = 25 + raw/127"
    },
    {
      start: 0x2e,
      len: 2,
      label:
        "L-JC: secondary thermal / status s16 LE @0x2E (small swing vs 0x2C; not the main temp field)"
    },
    { start: 0x30, len: 2, label: "IMU: accel X (s16, decoder @0x30)" },
    { start: 0x32, len: 2, label: "IMU: accel Y" },
    { start: 0x34, len: 2, label: "IMU: accel Z" },
    { start: 0x36, len: 2, label: "IMU: gyro X" },
    { start: 0x38, len: 2, label: "IMU: gyro Y" },
    { start: 0x3a, len: 2, label: "IMU: gyro Z" },
    { start: 0x3c, len: 1, label: "Analog L (if present)" },
    { start: 0x3d, len: 1, label: "Analog R (if present)" },
    {
      start: 0x2a,
      len: 18,
      label: "JC2: motion @0x2A（18B）：ts+temp+accel+gyro；accel/gyro 与解码 @0x30 一致"
    }
  ];

  /** @type {Record<string, Uint8Array | null>} */
  const lastSnapshotBySide = Object.create(null);
  /** @type {Record<string, Uint8Array | null>} */
  const lastRenderedBySide = Object.create(null);

  const MOUNT_ATTR = "data-packet-lab-init";
  const DECAY_ATTR = "data-pl-decay";
  const DECAY_ROW_ATTR = "data-pl-decay-row";
  /** Recent-change highlight: full strength at t=0, fades to none over this window. */
  const DECAY_MS = 10000;
  const DECAY_TICK_MS = 80;

  /** @type {{ left: Map<string, number>, right: Map<string, number> }} */
  const decayTimeBySide = {
    left: new Map(),
    right: new Map()
  };
  /** @type {HTMLElement[]} */
  const decayMountNodes = [];
  let decayIntervalId = null;

  function decayMap(sideKey) {
    return decayTimeBySide[sideKey] || decayTimeBySide.left;
  }

  function pruneDecayMap(sideKey, nBytes) {
    const m = decayMap(sideKey);
    for (const k of [...m.keys()]) {
      if (k.startsWith("h:")) {
        const i = Number(k.slice(2));
        if (!Number.isFinite(i) || i >= nBytes) {
          m.delete(k);
        }
      } else if (k.startsWith("vl:") || k.startsWith("vo:")) {
        const off = Number(k.split(":")[1]);
        if (!Number.isFinite(off) || off >= nBytes) {
          m.delete(k);
        }
      } else if (k.startsWith("v:")) {
        const off = Number(k.split(":")[1]);
        if (!Number.isFinite(off) || off >= nBytes) {
          m.delete(k);
        }
      } else if (k.startsWith("b0:") || k.startsWith("bh:") || k.startsWith("bl:")) {
        const bi = Number(k.split(":")[1]);
        if (!Number.isFinite(bi) || bi >= nBytes) {
          m.delete(k);
        }
      } else if (k.startsWith("b:")) {
        const bi = Number(k.split(":")[1]);
        if (!Number.isFinite(bi) || bi >= nBytes) {
          m.delete(k);
        }
      }
    }
  }

  function paintDecayElement(el, sideKey) {
    if (!(el instanceof HTMLElement)) {
      return;
    }
    if (shouldSuppressRedFlash(el)) {
      el.style.removeProperty("background-color");
      el.style.removeProperty("box-shadow");
      return;
    }
    const key = el.getAttribute(DECAY_ATTR);
    if (!key) {
      el.style.removeProperty("background-color");
      el.style.removeProperty("box-shadow");
      return;
    }
    const t0 = decayMap(sideKey).get(key);
    const now = Date.now();
    if (t0 == null || now - t0 >= DECAY_MS) {
      decayMap(sideKey).delete(key);
      el.style.removeProperty("background-color");
      el.style.removeProperty("box-shadow");
      return;
    }
    const s = 1 - (now - t0) / DECAY_MS;
    const a = 0.1 + 0.52 * s;
    const edge = 0.12 + 0.48 * s;
    el.style.backgroundColor = `rgba(248, 113, 113, ${a})`;
    el.style.boxShadow = `inset 0 0 0 1px rgba(220, 38, 38, ${edge})`;
  }

  function paintDecayRow(tr, sideKey) {
    if (!(tr instanceof HTMLElement)) {
      return;
    }
    if (shouldSuppressRedFlash(tr)) {
      tr.style.removeProperty("box-shadow");
      return;
    }
    const key = tr.getAttribute(DECAY_ROW_ATTR);
    if (!key) {
      tr.style.removeProperty("box-shadow");
      return;
    }
    const t0 = decayMap(sideKey).get(key);
    const now = Date.now();
    if (t0 == null || now - t0 >= DECAY_MS) {
      decayMap(sideKey).delete(key);
      tr.style.removeProperty("box-shadow");
      return;
    }
    const s = 1 - (now - t0) / DECAY_MS;
    tr.style.boxShadow = `inset 4px 0 0 rgba(239, 68, 68, ${0.22 + 0.72 * s})`;
  }

  function tickDecayAllMounts() {
    for (let i = decayMountNodes.length - 1; i >= 0; i--) {
      if (!decayMountNodes[i].isConnected) {
        decayMountNodes.splice(i, 1);
      }
    }
    for (const mount of decayMountNodes) {
      if (!mount.isConnected) {
        continue;
      }
      for (const wrap of mount.children) {
        if (!(wrap instanceof HTMLElement)) {
          continue;
        }
        const sideKey = wrap.dataset.plSide;
        if (!sideKey) {
          continue;
        }
        wrap.querySelectorAll(`[${DECAY_ATTR}]`).forEach((el) => paintDecayElement(el, sideKey));
        wrap.querySelectorAll(`[${DECAY_ROW_ATTR}]`).forEach((el) => paintDecayRow(el, sideKey));
      }
    }
  }

  function ensureDecayTicker() {
    if (decayIntervalId != null) {
      return;
    }
    decayIntervalId = window.setInterval(tickDecayAllMounts, DECAY_TICK_MS);
  }

  function registerDecayMount(mount) {
    if (decayMountNodes.indexOf(mount) === -1) {
      decayMountNodes.push(mount);
    }
    ensureDecayTicker();
  }

  function unregisterDecayMount(mount) {
    const i = decayMountNodes.indexOf(mount);
    if (i !== -1) {
      decayMountNodes.splice(i, 1);
    }
    if (decayMountNodes.length === 0 && decayIntervalId != null) {
      window.clearInterval(decayIntervalId);
      decayIntervalId = null;
    }
  }

  function clearDecayStylesInWrap(wrap, sideKey) {
    wrap.querySelectorAll(`[${DECAY_ATTR}]`).forEach((el) => {
      if (el instanceof HTMLElement) {
        el.removeAttribute(DECAY_ATTR);
        el.style.removeProperty("background-color");
        el.style.removeProperty("box-shadow");
      }
    });
    wrap.querySelectorAll(`[${DECAY_ROW_ATTR}]`).forEach((el) => {
      if (el instanceof HTMLElement) {
        el.removeAttribute(DECAY_ROW_ATTR);
        el.style.removeProperty("box-shadow");
      }
    });
    decayMap(sideKey).clear();
  }

  function byteHinted(index) {
    return PACKET_FIELD_HINTS.some((h) => index >= h.start && index < h.start + h.len);
  }

  function bytesEqual(a, b) {
    if (!a || !b || a.length !== b.length) {
      return false;
    }
    for (let i = 0; i < a.length; i++) {
      if (a[i] !== b[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * @param {Uint8Array | null | undefined} prev
   * @param {Uint8Array} next
   * @returns {boolean[]}
   */
  function computeByteChanged(prev, next) {
    const n = next.length;
    const changed = new Array(n).fill(false);
    if (!prev || prev.length === 0) {
      return changed;
    }
    const m = Math.min(prev.length, n);
    for (let i = 0; i < m; i++) {
      if (prev[i] !== next[i]) {
        changed[i] = true;
      }
    }
    for (let i = m; i < n; i++) {
      changed[i] = true;
    }
    return changed;
  }

  function anyByteChanged(changed, off, span) {
    const n = changed.length;
    for (let k = 0; k < span && off + k < n; k++) {
      if (changed[off + k]) {
        return true;
      }
    }
    return false;
  }

  const PIN_CLASS = "packet-lab-pin";

  function hasPin(el) {
    return el.classList.contains(PIN_CLASS);
  }

  /** Pinned cells / rows / hex rows skip red change tint. */
  function shouldSuppressRedFlash(el) {
    if (hasPin(el)) {
      return true;
    }
    const tr = el.closest("tr");
    if (tr && hasPin(tr)) {
      return true;
    }
    const hexRow = el.closest("tr.packet-hex-row");
    if (hexRow && hasPin(hexRow)) {
      return true;
    }
    return false;
  }

  function touchDecayKey(sideKey, key, active) {
    if (active && key) {
      decayMap(sideKey).set(key, Date.now());
    }
  }

  function setByteSpanState(td, hinted, changed, sideKey, byteIdx) {
    let base = "packet-byte";
    if (hinted) {
      base += " packet-byte--hint";
    }
    if (hasPin(td)) {
      base += " " + PIN_CLASS;
    }
    td.className = base;
    const dk = `h:${byteIdx}`;
    td.setAttribute(DECAY_ATTR, dk);
    touchDecayKey(sideKey, dk, changed);
    paintDecayElement(td, sideKey);
  }

  function setTdBaseAndDecay(td, baseClass, sideKey, decayKey, touched) {
    td.className = hasPin(td) ? `${baseClass} ${PIN_CLASS}` : baseClass;
    if (decayKey) {
      td.setAttribute(DECAY_ATTR, decayKey);
    }
    touchDecayKey(sideKey, decayKey, touched);
    paintDecayElement(td, sideKey);
  }

  function setValueDataCell(td, sideKey, off, j, touched) {
    const dk = `v:${off}:${j}`;
    td.className = hasPin(td) ? PIN_CLASS : "";
    td.setAttribute(DECAY_ATTR, dk);
    touchDecayKey(sideKey, dk, touched);
    paintDecayElement(td, sideKey);
  }

  function onPacketLabPinClick(wrap, e) {
    const t = e.target;
    if (!(t instanceof Element)) {
      return;
    }
    if (t.closest("a, button, input, select, textarea, label")) {
      return;
    }
    if (!wrap.contains(t)) {
      return;
    }

    if (t.closest(".packet-hex-table thead")) {
      return;
    }

    const hexByte = t.closest(".packet-hex-tbody td.packet-byte");
    if (hexByte instanceof HTMLElement && hexByte.innerText.trim() !== "") {
      e.preventDefault();
      const row = hexByte.closest("tr.packet-hex-row");
      row?.classList.remove(PIN_CLASS);
      hexByte.classList.toggle(PIN_CLASS);
      paintDecayElement(hexByte, wrap.dataset.plSide || "left");
      return;
    }

    const hexRowOff = t.closest(".packet-hex-tbody tr.packet-hex-row td.packet-off");
    if (hexRowOff) {
      e.preventDefault();
      const row = hexRowOff.closest("tr.packet-hex-row");
      if (!row) {
        return;
      }
      row.querySelectorAll("td.packet-byte").forEach((b) => {
        b.classList.remove(PIN_CLASS);
        paintDecayElement(b, wrap.dataset.plSide || "left");
      });
      row.classList.toggle(PIN_CLASS);
      return;
    }

    const vtd = t.closest(".packet-lab-table tbody td");
    if (vtd) {
      e.preventDefault();
      const tr = vtd.parentElement;
      if (vtd.cellIndex === 0) {
        [...tr.cells].forEach((c) => {
          c.classList.remove(PIN_CLASS);
          paintDecayElement(c, wrap.dataset.plSide || "left");
        });
        tr.classList.toggle(PIN_CLASS);
        paintDecayRow(tr, wrap.dataset.plSide || "left");
      } else {
        tr.classList.remove(PIN_CLASS);
        vtd.classList.toggle(PIN_CLASS);
        paintDecayElement(vtd, wrap.dataset.plSide || "left");
        paintDecayRow(tr, wrap.dataset.plSide || "left");
      }
      return;
    }

    const btd = t.closest(".packet-bit-table tbody td");
    if (btd) {
      e.preventDefault();
      const tr = btd.parentElement;
      if (btd.cellIndex === 0) {
        [...tr.cells].forEach((c) => {
          c.classList.remove(PIN_CLASS);
          paintDecayElement(c, wrap.dataset.plSide || "left");
        });
        tr.classList.toggle(PIN_CLASS);
        paintDecayRow(tr, wrap.dataset.plSide || "left");
      } else {
        tr.classList.remove(PIN_CLASS);
        btd.classList.toggle(PIN_CLASS);
        paintDecayElement(btd, wrap.dataset.plSide || "left");
        paintDecayRow(tr, wrap.dataset.plSide || "left");
      }
    }
  }

  function attachPacketLabPinClicks(wrap) {
    wrap.addEventListener("click", (e) => onPacketLabPinClick(wrap, e));
  }

  function createSection(classExtra, titleText) {
    const section = document.createElement("section");
    section.className = `packet-lab-block ${classExtra}`;
    const h4 = document.createElement("h4");
    h4.className = "packet-lab-block-title";
    h4.innerText = titleText;
    section.appendChild(h4);
    return section;
  }

  function createSideShell(strings) {
    const wrap = document.createElement("div");
    wrap.className = "packet-lab-side";

    const h = document.createElement("h3");
    h.className = "packet-lab-heading";
    wrap.appendChild(h);

    const meta = document.createElement("p");
    meta.className = "muted packet-lab-meta";
    wrap.appendChild(meta);

    const noData = document.createElement("p");
    noData.className = "muted packet-lab-nodata";
    noData.innerText = strings.noData;
    wrap.appendChild(noData);

    const stack = document.createElement("div");
    stack.className = "packet-lab-stack";
    stack.hidden = true;

    const blockHex = createSection("packet-lab-block--hex", strings.sectionHex || "Hex");
    const hexPanel = document.createElement("div");
    hexPanel.className = "packet-hex packet-hex--panel";
    const hexTable = document.createElement("table");
    hexTable.className = "packet-hex-table";
    const colgroup = document.createElement("colgroup");
    const colOff = document.createElement("col");
    colOff.className = "packet-hex-col-off";
    colgroup.appendChild(colOff);
    for (let c = 0; c < 16; c++) {
      const col = document.createElement("col");
      col.className = "packet-hex-col-nib";
      colgroup.appendChild(col);
    }
    hexTable.appendChild(colgroup);
    const hexThead = document.createElement("thead");
    const hexHeadTr = document.createElement("tr");
    const thOff = document.createElement("th");
    thOff.className = "packet-off packet-hex-th-off";
    thOff.innerText = "off";
    hexHeadTr.appendChild(thOff);
    for (let i = 0; i < 16; i++) {
      const th = document.createElement("th");
      th.className = "packet-hex-th-nib";
      th.innerText = i.toString(16);
      hexHeadTr.appendChild(th);
    }
    hexThead.appendChild(hexHeadTr);
    const hexTbody = document.createElement("tbody");
    hexTbody.className = "packet-hex-tbody";
    hexTable.appendChild(hexThead);
    hexTable.appendChild(hexTbody);
    hexPanel.appendChild(hexTable);
    blockHex.appendChild(hexPanel);
    stack.appendChild(blockHex);

    const blockValues = createSection("packet-lab-block--values", strings.sectionValues || "Values");
    const scrollVal = document.createElement("div");
    scrollVal.className = "packet-lab-scroll packet-lab-scroll--panel";
    const valTable = document.createElement("table");
    valTable.className = "packet-lab-table";
    valTable.innerHTML =
      "<thead><tr><th>off</th><th>u8</th><th>i8</th><th>u16</th><th>i16</th><th>u32</th><th>i32</th></tr></thead><tbody></tbody>";
    scrollVal.appendChild(valTable);
    blockValues.appendChild(scrollVal);
    stack.appendChild(blockValues);

    const blockBits = createSection("packet-lab-block--bits", strings.sectionBits || "Bits");
    const scrollBits = document.createElement("div");
    scrollBits.className = "packet-lab-scroll packet-lab-scroll--panel packet-bit-scroll";
    const bitTable = document.createElement("table");
    bitTable.className = "packet-bit-table";
    const bitHead = [7, 6, 5, 4, 3, 2, 1, 0].map((b) => `<th class="packet-bit-th">b${b}</th>`).join("");
    bitTable.innerHTML = `<thead><tr><th>off</th>${bitHead}<th>hex</th></tr></thead><tbody></tbody>`;
    scrollBits.appendChild(bitTable);
    blockBits.appendChild(scrollBits);
    stack.appendChild(blockBits);

    wrap.appendChild(stack);

    if (strings.diffHint) {
      const hint = document.createElement("p");
      hint.className = "muted packet-lab-diff-hint";
      hint.innerText = strings.diffHint;
      wrap.appendChild(hint);
    }

    if (strings.pinHint) {
      const pinHint = document.createElement("p");
      pinHint.className = "muted packet-lab-pin-hint";
      pinHint.innerText = strings.pinHint;
      wrap.appendChild(pinHint);
    }

    attachPacketLabPinClicks(wrap);

    return wrap;
  }

  function ensureHexRowStructure(tbody, rowIndex) {
    while (tbody.rows.length <= rowIndex) {
      const tr = document.createElement("tr");
      tr.className = "packet-hex-row";
      const off = document.createElement("td");
      off.className = "packet-off";
      tr.appendChild(off);
      for (let c = 0; c < 16; c++) {
        const td = document.createElement("td");
        td.className = "packet-byte";
        tr.appendChild(td);
      }
      tbody.appendChild(tr);
    }
  }

  function trimHexRows(tbody, nRows) {
    while (tbody.rows.length > nRows) {
      tbody.removeChild(tbody.lastElementChild);
    }
  }

  /**
   * @param {HTMLTableSectionElement} tbody
   * @param {Uint8Array} bytes
   * @param {boolean[]} changed
   * @param {string} sideKey
   */
  function patchHex(tbody, bytes, changed, sideKey) {
    const per = 16;
    const n = bytes.length;
    const nLines = n === 0 ? 0 : Math.ceil(n / per);
    trimHexRows(tbody, nLines);
    for (let r = 0; r < nLines; r++) {
      ensureHexRowStructure(tbody, r);
      const tr = tbody.rows[r];
      const offEl = tr.cells[0];
      const rowOff = r * per;
      const offText = rowOff.toString(16).padStart(4, "0");
      if (offEl.innerText !== offText) {
        offEl.innerText = offText;
      }
      for (let c = 0; c < 16; c++) {
        const td = tr.cells[1 + c];
        const idx = rowOff + c;
        if (idx >= n) {
          if (td.innerText !== "") {
            td.innerText = "";
          }
          td.className = "packet-byte";
          td.classList.remove(PIN_CLASS);
          td.removeAttribute(DECAY_ATTR);
          td.style.removeProperty("background-color");
          td.style.removeProperty("box-shadow");
          td.removeAttribute("hidden");
          decayMap(sideKey).delete(`h:${idx}`);
          continue;
        }
        td.removeAttribute("hidden");
        const hx = bytes[idx].toString(16).padStart(2, "0");
        if (td.innerText !== hx) {
          td.innerText = hx;
        }
        setByteSpanState(td, byteHinted(idx), changed[idx], sideKey, idx);
      }
    }
  }

  function ensureValueRows(tbody, n) {
    while (tbody.rows.length < n) {
      const tr = tbody.insertRow();
      tr.className = "packet-value-row";
      for (let k = 0; k < 7; k++) {
        tr.insertCell();
      }
    }
    while (tbody.rows.length > n) {
      tbody.deleteRow(tbody.rows.length - 1);
    }
  }

  /**
   * @param {HTMLTableSectionElement} tbody
   * @param {Uint8Array} bytes
   * @param {boolean[]} changed
   * @param {string} sideKey
   */
  function patchValueTable(tbody, bytes, changed, sideKey) {
    const n = bytes.length;
    if (n === 0) {
      while (tbody.rows.length > 0) {
        tbody.deleteRow(0);
      }
      return;
    }
    const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    const dash = "—";
    ensureValueRows(tbody, n);
    for (let off = 0; off < n; off++) {
      const tr = tbody.rows[off];
      const u8 = bytes[off];
      const i8 = u8 > 127 ? u8 - 256 : u8;
      let u16 = dash;
      let i16 = dash;
      let u32 = dash;
      let i32 = dash;
      if (off + 2 <= n) {
        u16 = String(dv.getUint16(off, true));
        i16 = String(dv.getInt16(off, true));
      }
      if (off + 4 <= n) {
        u32 = String(dv.getUint32(off, true));
        i32 = String(dv.getInt32(off, true));
      }

      const d0 = anyByteChanged(changed, off, 1);
      const d2 = off + 2 <= n && anyByteChanged(changed, off, 2);
      const d4 = off + 4 <= n && anyByteChanged(changed, off, 4);

      let trCls = "packet-value-row";
      if (byteHinted(off)) {
        trCls += " packet-row--hint";
      }
      if (hasPin(tr)) {
        trCls += " " + PIN_CLASS;
      }
      tr.className = trCls;
      tr.setAttribute(DECAY_ROW_ATTR, `vl:${off}`);
      touchDecayKey(sideKey, `vl:${off}`, d0 || d2 || d4);
      paintDecayRow(tr, sideKey);

      const offText = `0x${off.toString(16)}`;
      const cells = tr.cells;
      const c0 = cells[0];
      if (c0.innerText !== offText) {
        c0.innerText = offText;
      }
      setTdBaseAndDecay(c0, "packet-off", sideKey, `vo:${off}`, d0);

      const texts = [String(u8), String(i8), u16, i16, u32, i32];
      const diffs = [d0, d0, d2, d2, d4, d4];
      for (let j = 0; j < 6; j++) {
        const td = cells[j + 1];
        const t = texts[j];
        if (td.innerText !== t) {
          td.innerText = t;
        }
        setValueDataCell(td, sideKey, off, j, diffs[j]);
      }
    }
  }

  function ensureBitRows(tbody, n) {
    while (tbody.rows.length < n) {
      const tr = document.createElement("tr");
      tr.className = "packet-bit-row";
      const offTd = document.createElement("td");
      offTd.className = "packet-off";
      tr.appendChild(offTd);
      for (let b = 0; b < 8; b++) {
        tr.appendChild(document.createElement("td"));
      }
      const hexTd = document.createElement("td");
      hexTd.className = "packet-bit-hex";
      const code = document.createElement("code");
      hexTd.appendChild(code);
      tr.appendChild(hexTd);
      tbody.appendChild(tr);
    }
    while (tbody.rows.length > n) {
      tbody.removeChild(tbody.lastElementChild);
    }
  }

  /**
   * @param {HTMLTableSectionElement} tbody
   * @param {Uint8Array} bytes
   * @param {Uint8Array | null} prev
   * @param {boolean[]} changed
   * @param {string} sideKey
   */
  function patchBitTable(tbody, bytes, prev, changed, sideKey) {
    const n = bytes.length;
    if (n === 0) {
      while (tbody.rows.length > 0) {
        tbody.removeChild(tbody.lastElementChild);
      }
      return;
    }
    ensureBitRows(tbody, n);
    for (let i = 0; i < n; i++) {
      const tr = tbody.rows[i];
      const v = bytes[i];
      const offText = `0x${i.toString(16)}`;
      const offTd = tr.cells[0];
      if (offTd.innerText !== offText) {
        offTd.innerText = offText;
      }
      setTdBaseAndDecay(offTd, "packet-off", sideKey, `b0:${i}`, changed[i]);

      for (let bi = 0; bi < 8; bi++) {
        const b = 7 - bi;
        const bit = (v >> b) & 1;
        const td = tr.cells[1 + bi];
        let flip = false;
        if (prev && i < prev.length) {
          flip = ((prev[i] >> b) & 1) !== bit;
        }
        const t = String(bit);
        if (td.innerText !== t) {
          td.innerText = t;
        }
        const bitPin = hasPin(td) ? ` ${PIN_CLASS}` : "";
        td.className = `packet-bit-cell packet-bit-cell--${bit}${bitPin}`;
        const dk = `b:${i}:${bi}`;
        td.setAttribute(DECAY_ATTR, dk);
        touchDecayKey(sideKey, dk, flip);
        paintDecayElement(td, sideKey);
      }

      const hexTd = tr.cells[9];
      const code = hexTd.querySelector("code");
      const hx = v.toString(16).padStart(2, "0");
      if (code && code.innerText !== hx) {
        code.innerText = hx;
      }
      hexTd.className = hasPin(hexTd) ? `packet-bit-hex ${PIN_CLASS}` : "packet-bit-hex";
      hexTd.setAttribute(DECAY_ATTR, `bh:${i}`);
      touchDecayKey(sideKey, `bh:${i}`, changed[i]);
      paintDecayElement(hexTd, sideKey);

      let rowCls = "packet-bit-row";
      if (byteHinted(i)) {
        rowCls += " packet-bit-row--hint";
      }
      if (hasPin(tr)) {
        rowCls += " " + PIN_CLASS;
      }
      tr.className = rowCls;
      tr.setAttribute(DECAY_ROW_ATTR, `bl:${i}`);
      touchDecayKey(sideKey, `bl:${i}`, changed[i]);
      paintDecayRow(tr, sideKey);
    }
  }

  /**
   * @param {HTMLElement} mount
   * @param {*} strings
   */
  function initPacketLabMount(mount, strings) {
    if (mount.getAttribute(MOUNT_ATTR) === "1") {
      return;
    }
    mount.replaceChildren(createSideShell(strings), createSideShell(strings));
    const a = mount.children[0];
    const b = mount.children[1];
    if (a instanceof HTMLElement) {
      a.dataset.plSide = "left";
    }
    if (b instanceof HTMLElement) {
      b.dataset.plSide = "right";
    }
    mount.setAttribute(MOUNT_ATTR, "1");
    registerDecayMount(mount);
  }

  function invalidatePacketLabMount(mount) {
    if (mount) {
      mount.removeAttribute(MOUNT_ATTR);
      unregisterDecayMount(mount);
    }
  }

  /**
   * @param {string} sideKey
   * @param {HTMLElement} wrap
   * @param {string} title
   * @param {*} sideState
   * @param {*} strings
   */
  function updatePacketLabSide(sideKey, wrap, title, sideState, strings) {
    const h3 = wrap.querySelector(".packet-lab-heading");
    if (h3 && h3.innerText !== title) {
      h3.innerText = title;
    }

    const meta = wrap.querySelector(".packet-lab-meta");
    const noData = wrap.querySelector(".packet-lab-nodata");
    const stack = wrap.querySelector(".packet-lab-stack");
    const hexTbody = wrap.querySelector(".packet-hex-tbody");
    const valTbody = wrap.querySelector(".packet-lab-table tbody");
    const bitTbody = wrap.querySelector(".packet-bit-table tbody");

    const raw = sideState.raw;
    if (!raw || !raw.length) {
      lastSnapshotBySide[sideKey] = null;
      lastRenderedBySide[sideKey] = null;
      clearDecayStylesInWrap(wrap, sideKey);
      if (stack) {
        stack.hidden = true;
      }
      if (noData) {
        noData.hidden = false;
        if (noData.innerText !== strings.noData) {
          noData.innerText = strings.noData;
        }
      }
      if (meta) {
        meta.innerText = "";
      }
      return;
    }

    const bytes = raw instanceof Uint8Array ? raw : new Uint8Array(raw);
    if (lastRenderedBySide[sideKey] && bytesEqual(lastRenderedBySide[sideKey], bytes)) {
      return;
    }

    const prevSnap = lastSnapshotBySide[sideKey];
    const changed = computeByteChanged(prevSnap, bytes);

    if (noData) {
      noData.hidden = true;
    }
    if (stack) {
      stack.hidden = false;
    }

    const metaText =
      typeof strings.bytesLabelFor === "function"
        ? strings.bytesLabelFor(bytes.length)
        : String(strings.bytesLabel || "").replace("{n}", String(bytes.length));
    if (meta && meta.innerText !== metaText) {
      meta.innerText = metaText;
    }

    pruneDecayMap(sideKey, bytes.length);

    if (hexTbody) {
      patchHex(hexTbody, bytes, changed, sideKey);
    }
    if (valTbody) {
      patchValueTable(valTbody, bytes, changed, sideKey);
    }
    if (bitTbody) {
      patchBitTable(bitTbody, bytes, prevSnap, changed, sideKey);
    }

    lastSnapshotBySide[sideKey] = new Uint8Array(bytes);
    lastRenderedBySide[sideKey] = new Uint8Array(bytes);
  }

  function syncPacketLab(mount, leftTitle, rightTitle, leftState, rightState, strings) {
    initPacketLabMount(mount, strings);
    const kids = mount.children;
    if (kids.length >= 2) {
      updatePacketLabSide("left", kids[0], leftTitle, leftState, strings);
      updatePacketLabSide("right", kids[1], rightTitle, rightState, strings);
    }
  }

  function clearSnapshots() {
    lastSnapshotBySide.left = null;
    lastSnapshotBySide.right = null;
    lastRenderedBySide.left = null;
    lastRenderedBySide.right = null;
    decayTimeBySide.left.clear();
    decayTimeBySide.right.clear();
  }

  window.PacketLab = {
    syncPacketLab,
    initPacketLabMount,
    invalidatePacketLabMount,
    updatePacketLabSide,
    clearSnapshots,
    PACKET_FIELD_HINTS
  };
})();
