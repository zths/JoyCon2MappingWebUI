const { t, setLanguage, getLanguage, subscribe: onLanguageChange } = window.I18n;

const actionOptions = [
  { value: "none", labelKey: "actions.none" },
  { value: "mouse_left", labelKey: "actions.mouseLeft" },
  { value: "mouse_right", labelKey: "actions.mouseRight" },
  { value: "mouse_middle", labelKey: "actions.mouseMiddle" },
  { value: "key_space", labelKey: "actions.keySpace" },
  { value: "key_enter", labelKey: "actions.keyEnter" },
  { value: "key_escape", labelKey: "actions.keyEscape" },
  { value: "key_tab", labelKey: "actions.keyTab" },
  { value: "key_ctrl", labelKey: "actions.keyCtrl" },
  { value: "key_shift", labelKey: "actions.keyShift" },
  { value: "key_alt", labelKey: "actions.keyAlt" },
  { value: "key_up", labelKey: "actions.keyUp" },
  { value: "key_down", labelKey: "actions.keyDown" },
  { value: "key_left", labelKey: "actions.keyLeft" },
  { value: "key_right", labelKey: "actions.keyRight" },
  { value: "key_w", labelKey: "actions.keyW" },
  { value: "key_a", labelKey: "actions.keyA" },
  { value: "key_s", labelKey: "actions.keyS" },
  { value: "key_d", labelKey: "actions.keyD" },
  { value: "key_q", labelKey: "actions.keyQ" },
  { value: "key_e", labelKey: "actions.keyE" },
  { value: "key_r", labelKey: "actions.keyR" },
  { value: "key_f", labelKey: "actions.keyF" },
  { value: "key_1", labelKey: "actions.key1" },
  { value: "key_2", labelKey: "actions.key2" },
  { value: "key_3", labelKey: "actions.key3" },
  { value: "key_4", labelKey: "actions.key4" },
  { value: "key_5", labelKey: "actions.key5" },
  { value: "key_custom", labelKey: "actions.keyCustom" }
];

const ui = {
  leftStatus: document.getElementById("leftStatus"),
  rightStatus: document.getElementById("rightStatus"),
  leftName: document.getElementById("leftName"),
  rightName: document.getElementById("rightName"),
  leftStats: document.getElementById("leftStats"),
  rightStats: document.getElementById("rightStats"),
  mouseStats: document.getElementById("mouseStats"),
  interactiveMapper: document.getElementById("interactiveMapper"),
  languageSelect: document.getElementById("languageSelect"),
  serverSettingsBtn: document.getElementById("serverSettingsBtn"),
  saveConfigBtn: document.getElementById("saveConfigBtn"),
  downloadConfigBtn: document.getElementById("downloadConfigBtn"),
  loadConfigBtn: document.getElementById("loadConfigBtn"),
  importConfigInput: document.getElementById("importConfigInput"),
  editorDock: document.getElementById("editorDock"),
  dockEditorKicker: document.getElementById("dockEditorKicker"),
  dockEditorTitle: document.getElementById("dockEditorTitle"),
  dockEditorContent: document.getElementById("dockEditorContent"),
  editorModal: document.getElementById("editorModal"),
  editorBackdrop: document.getElementById("editorBackdrop"),
  editorKicker: document.getElementById("editorKicker"),
  editorTitle: document.getElementById("editorTitle"),
  editorContent: document.getElementById("editorContent"),
  closeEditorBtn: document.getElementById("closeEditorBtn")
};

const stickDirectionEntries = [
  { id: "up", labelKey: "directions.up" },
  { id: "left", labelKey: "directions.left" },
  { id: "down", labelKey: "directions.down" },
  { id: "right", labelKey: "directions.right" }
];

const defaultStickConfig = {
  deadzone: 8000,
  hysteresis: 1600,
  diagonalUnlockRadius: 14000,
  fourWayHysteresisDegrees: 12,
  eightWayHysteresisDegrees: 8,
  up: "none",
  down: "none",
  left: "none",
  right: "none"
};

const defaultMouseConfig = {
  enabled: true,
  baseSensitivity: 0.10,
  acceleration: 0.040,
  exponent: 0.50,
  maxGain: 2.50,
  distanceThreshold: 12
};

const defaultServerConfig = {
  port: 17777
};

const hotspots = {
  left: [
    { id: "left-zl", side: "left", type: "button", runtimeId: "ZL", labelKey: "hotspots.leftZl", customClass: "hotspot-arc-zl" },
    { id: "left-l", side: "left", type: "button", runtimeId: "L", labelKey: "hotspots.leftL", customClass: "hotspot-arc-l" },
    { id: "left-minus", side: "left", type: "button", runtimeId: "Minus", labelKey: "hotspots.leftMinus", x: 130, y: 43, w: 32, h: 32, shape: "circle" },
    { id: "left-stick", side: "left", type: "stick", runtimeId: "left", labelKey: "hotspots.leftStick", x: 50, y: 104, w: 82, h: 82, shape: "circle" },
    { id: "left-mouse", side: "left", type: "mouse", runtimeId: "left", labelKey: "hotspots.leftMouse", x: 182, y: 259, w: 15, h: 51, shape: "pill" },
    { id: "left-up", side: "left", type: "button", runtimeId: "Up", labelKey: "hotspots.leftUp", x: 73, y: 248, w: 36, h: 36, shape: "circle" },
    { id: "left-left", side: "left", type: "button", runtimeId: "Left", labelKey: "hotspots.leftLeft", x: 29, y: 292, w: 36, h: 36, shape: "circle" },
    { id: "left-right", side: "left", type: "button", runtimeId: "Right", labelKey: "hotspots.leftRight", x: 117, y: 292, w: 36, h: 36, shape: "circle" },
    { id: "left-down", side: "left", type: "button", runtimeId: "Down", labelKey: "hotspots.leftDown", x: 73, y: 336, w: 36, h: 36, shape: "circle" },
    { id: "left-capture", side: "left", type: "button", runtimeId: "Capture", labelKey: "hotspots.leftCapture", x: 118, y: 420, w: 28, h: 28, shape: "square" },
    { id: "left-sl", side: "left", type: "button", runtimeId: "SL", labelKey: "hotspots.leftSl", x: 186, y: 90, w: 10, h: 82, shape: "pill" },
    { id: "left-sr", side: "left", type: "button", runtimeId: "SR", labelKey: "hotspots.leftSr", x: 186, y: 380, w: 10, h: 82, shape: "pill" }
  ],
  right: [
    { id: "right-zr", side: "right", type: "button", runtimeId: "ZR", labelKey: "hotspots.rightZr", customClass: "hotspot-arc-zr" },
    { id: "right-r", side: "right", type: "button", runtimeId: "R", labelKey: "hotspots.rightR", customClass: "hotspot-arc-r" },
    { id: "right-mouse", side: "right", type: "mouse", runtimeId: "right", labelKey: "hotspots.rightMouse", x: 1, y: 259, w: 15, h: 51, shape: "pill" },
    { id: "right-plus", side: "right", type: "button", runtimeId: "Plus", labelKey: "hotspots.rightPlus", x: 43, y: 44, w: 32, h: 32, shape: "circle" },
    { id: "right-x", side: "right", type: "button", runtimeId: "X", labelKey: "hotspots.rightX", x: 91, y: 83, w: 36, h: 36, shape: "circle" },
    { id: "right-y", side: "right", type: "button", runtimeId: "Y", labelKey: "hotspots.rightY", x: 47, y: 127, w: 36, h: 36, shape: "circle" },
    { id: "right-a", side: "right", type: "button", runtimeId: "A", labelKey: "hotspots.rightA", x: 135, y: 127, w: 36, h: 36, shape: "circle" },
    { id: "right-b", side: "right", type: "button", runtimeId: "B", labelKey: "hotspots.rightB", x: 91, y: 171, w: 36, h: 36, shape: "circle" },
    { id: "right-stick", side: "right", type: "stick", runtimeId: "right", labelKey: "hotspots.rightStick", x: 68, y: 274, w: 82, h: 82, shape: "circle" },
    { id: "right-home", side: "right", type: "button", runtimeId: "Home", labelKey: "hotspots.rightHome", x: 50, y: 410, w: 36, h: 36, shape: "circle" },
    { id: "right-c", side: "right", type: "button", runtimeId: "C", labelKey: "hotspots.rightC", x: 54, y: 466, w: 28, h: 28, shape: "square" },
    { id: "right-sl", side: "right", type: "button", runtimeId: "SL", labelKey: "hotspots.rightSl", x: 4, y: 90, w: 10, h: 82, shape: "pill" },
    { id: "right-sr", side: "right", type: "button", runtimeId: "SR", labelKey: "hotspots.rightSr", x: 4, y: 380, w: 10, h: 82, shape: "pill" }
  ]
};

let latestState = null;
let lastRenderedVisualKey = "";
let lastRenderedPopupKey = "";
let mouseDraftDirty = { left: false, right: false };
let mouseDraftConfig = { left: null, right: null };
let serverDraftDirty = false;
let serverDraftConfig = null;
let selectedHotspotId = null;
let toastTimer = null;
let refreshTimer = null;
let redirectInFlight = false;
const dockedEditorMedia = window.matchMedia("(min-width: 1100px)");

function formatDistanceThreshold(value) {
  return `0x${Number(value).toString(16)}`;
}

function ensureToast() {
  let toast = document.getElementById("appToast");
  if (!toast) {
    toast = document.createElement("div");
    toast.id = "appToast";
    toast.className = "app-toast";
    toast.setAttribute("role", "status");
    toast.setAttribute("aria-live", "polite");
    document.body.appendChild(toast);
  }
  return toast;
}

function showToast(message) {
  const toast = ensureToast();
  toast.textContent = message;
  toast.classList.add("visible");

  if (toastTimer) {
    clearTimeout(toastTimer);
  }

  toastTimer = window.setTimeout(() => {
    toast.classList.remove("visible");
    toastTimer = null;
  }, 2600);
}

function getActionLabel(option) {
  return t(option.labelKey);
}

function getHotspotLabel(hotspot) {
  return t(hotspot.labelKey);
}

function translateStatusValue(status) {
  const normalized = String(status || "").toLowerCase();
  return t(`statusValue.${normalized}`, {}, status || "-");
}

function applyStatusPillState(element, status) {
  const normalized = String(status || "").toLowerCase();
  element.classList.remove(
    "status-connected",
    "status-connecting",
    "status-disconnected",
    "status-error",
    "status-unknown"
  );

  if (["connected", "connecting", "disconnected", "error"].includes(normalized)) {
    element.classList.add(`status-${normalized}`);
    return;
  }

  element.classList.add("status-unknown");
}

function applyStaticTranslations() {
  document.documentElement.lang = getLanguage();
  document.querySelectorAll("[data-i18n]").forEach((element) => {
    element.textContent = t(element.dataset.i18n);
  });
  document.querySelectorAll("[data-i18n-aria-label]").forEach((element) => {
    element.setAttribute("aria-label", t(element.dataset.i18nAriaLabel));
  });
  if (ui.languageSelect) {
    ui.languageSelect.value = getLanguage();
  }
}

async function api(path, options = {}) {
  const response = await fetch(path, {
    method: options.method || "GET",
    headers: { "Content-Type": "application/json" },
    body: options.body ? JSON.stringify(options.body) : undefined
  });
  return response.json();
}

function getStickConfig(config, side) {
  const direct = side === "left" ? config.leftStick : config.rightStick;
  const nested = config.sticks?.[side];
  const source = nested || direct || {};
  return {
    deadzone: Number(source.deadzone ?? defaultStickConfig.deadzone),
    hysteresis: Number(source.hysteresis ?? defaultStickConfig.hysteresis),
    diagonalUnlockRadius: Number(source.diagonalUnlockRadius ?? source.cardinalLockRadius ?? defaultStickConfig.diagonalUnlockRadius),
    fourWayHysteresisDegrees: Number(source.fourWayHysteresisDegrees ?? defaultStickConfig.fourWayHysteresisDegrees),
    eightWayHysteresisDegrees: Number(source.eightWayHysteresisDegrees ?? defaultStickConfig.eightWayHysteresisDegrees),
    up: source.up ?? defaultStickConfig.up,
    down: source.down ?? defaultStickConfig.down,
    left: source.left ?? defaultStickConfig.left,
    right: source.right ?? defaultStickConfig.right
  };
}

function getMouseConfig(config, side) {
  const source = config.mouse?.[side] || {};
  return {
    enabled: source.enabled ?? defaultMouseConfig.enabled,
    baseSensitivity: Number(source.baseSensitivity ?? defaultMouseConfig.baseSensitivity),
    acceleration: Number(source.acceleration ?? defaultMouseConfig.acceleration),
    exponent: Number(source.exponent ?? defaultMouseConfig.exponent),
    maxGain: Number(source.maxGain ?? defaultMouseConfig.maxGain),
    distanceThreshold: Number(source.distanceThreshold ?? defaultMouseConfig.distanceThreshold)
  };
}

function normalizePort(value) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) {
    return defaultServerConfig.port;
  }
  return Math.max(1, Math.min(65535, Math.round(parsed)));
}

function getServerConfig(config) {
  return {
    port: normalizePort(config.server?.port ?? defaultServerConfig.port)
  };
}

function getEffectiveMouseConfig(side) {
  if (mouseDraftDirty[side] && mouseDraftConfig[side]) {
    return structuredClone(mouseDraftConfig[side]);
  }
  return getMouseConfig(latestState?.config || {}, side);
}

function getEffectiveServerConfig() {
  if (serverDraftDirty && serverDraftConfig) {
    return structuredClone(serverDraftConfig);
  }
  return getServerConfig(latestState?.config || {});
}

function buildLocalUrl(port) {
  const host = window.location.hostname || "127.0.0.1";
  return `${window.location.protocol}//${host}:${normalizePort(port)}/`;
}

function buildBrowserConfig() {
  const base = structuredClone(latestState?.config || {});
  base.mouse = {
    left: getEffectiveMouseConfig("left"),
    right: getEffectiveMouseConfig("right")
  };
  base.server = getEffectiveServerConfig();
  base.sticks = {
    left: getStickConfig(base, "left"),
    right: getStickConfig(base, "right")
  };
  delete base.leftStick;
  delete base.rightStick;
  return base;
}

async function saveAndApplyBrowserConfig(config) {
  const nextPort = getServerConfig(config).port;
  const currentPort = window.location.port ? normalizePort(window.location.port) : defaultServerConfig.port;
  await api("/api/config/replace", {
    method: "POST",
    body: config
  });
  await api("/api/config/save", { method: "POST" });

  if (nextPort !== currentPort) {
    redirectInFlight = true;
    if (refreshTimer) {
      clearInterval(refreshTimer);
      refreshTimer = null;
    }
    showToast(t("prompt.redirectingToNewPort", { port: nextPort }));
    window.setTimeout(() => {
      window.location.assign(buildLocalUrl(nextPort));
    }, 900);
    return { redirected: true };
  }

  return { redirected: false };
}

function normalizeConfig(config) {
  return {
    ...config,
    mouse: {
      left: getMouseConfig(config, "left"),
      right: getMouseConfig(config, "right")
    },
    server: getServerConfig(config),
    mapping: {
      left: config.mapping?.left || {},
      right: config.mapping?.right || {}
    },
    sticks: {
      left: getStickConfig(config, "left"),
      right: getStickConfig(config, "right")
    }
  };
}

function applyConfigToUi(config) {
  const normalized = normalizeConfig(config);
  latestState = {
    ...(latestState || {}),
    config: normalized
  };
  mouseDraftDirty = { left: false, right: false };
  mouseDraftConfig = { left: null, right: null };
  serverDraftDirty = false;
  serverDraftConfig = null;
  lastRenderedVisualKey = "";
  lastRenderedPopupKey = "";
  renderInteractiveMapper(true);
  renderEditor(true);
}

function formatControllerStats(sideState) {
  return [
    `${t("stats.controller.status")}: ${translateStatusValue(sideState.status)}`,
    `${t("stats.controller.device")}: ${sideState.deviceName || "-"}`,
    `${t("stats.controller.packets")}: ${sideState.packetCount}`,
    `${t("stats.controller.rateHz")}: ${sideState.rateHz.toFixed(2)}`,
    `${t("stats.controller.avgIntervalMs")}: ${sideState.averageIntervalMs.toFixed(3)}`,
    `${t("stats.controller.buttonBits")}: 0x${Number(sideState.buttonBits || 0).toString(16)}`,
    `${t("stats.controller.opticalDistance")}: 0x${Number(sideState.opticalDistance || 0).toString(16)}`,
    `${t("stats.controller.optical")}: (${sideState.opticalX}, ${sideState.opticalY})`,
    `${t("stats.controller.leftStick")}: (${sideState.stickLX}, ${sideState.stickLY})`,
    `${t("stats.controller.rightStick")}: (${sideState.stickRX}, ${sideState.stickRY})`,
    sideState.error ? `${t("stats.controller.error")}: ${sideState.error}` : ""
  ].filter(Boolean).join("\n");
}

function formatMouseStats(mouseStats) {
  return [
    `${t("stats.mouse.movedPackets")}: ${mouseStats.movedPackets}`,
    `${t("stats.mouse.injectedMoves")}: ${mouseStats.injectedMoves}`,
    `${t("stats.mouse.gatedPackets")}: ${mouseStats.gatedPackets}`,
    `${t("stats.mouse.avgDispatchUs")}: ${mouseStats.averageDispatchUs.toFixed(2)}`,
    `${t("stats.mouse.maxDispatchUs")}: ${mouseStats.maxDispatchUs.toFixed(2)}`,
    `${t("stats.mouse.distanceLast")}: 0x${Number(mouseStats.lastDistance).toString(16)}`,
    `${t("stats.mouse.distanceMin")}: 0x${Number(mouseStats.minDistance).toString(16)}`,
    `${t("stats.mouse.distanceMax")}: 0x${Number(mouseStats.maxDistance).toString(16)}`
  ].join("\n");
}

function buildVisualRenderKey(config) {
  return JSON.stringify({
    selectedHotspotId,
    mapping: config.mapping,
    sticks: {
      left: getStickConfig(config, "left"),
      right: getStickConfig(config, "right")
    }
  });
}

function buildPopupRenderKey(config) {
  return JSON.stringify({
    docked: dockedEditorMedia.matches,
    selectedHotspotId,
    mouse: {
      left: getEffectiveMouseConfig("left"),
      right: getEffectiveMouseConfig("right")
    },
    server: getEffectiveServerConfig(),
    mapping: config.mapping,
    sticks: {
      left: getStickConfig(config, "left"),
      right: getStickConfig(config, "right")
    }
  });
}

function findHotspot(id = selectedHotspotId) {
  if (!id) {
    return null;
  }
  if (id === "server-settings") {
    return {
      id,
      type: "server"
    };
  }
  return [...hotspots.left, ...hotspots.right].find((item) => item.id === id) || null;
}

function useDockedEditor() {
  return dockedEditorMedia.matches;
}

function getActiveEditorUi() {
  if (useDockedEditor()) {
    return {
      isDocked: true,
      kicker: ui.dockEditorKicker,
      title: ui.dockEditorTitle,
      content: ui.dockEditorContent
    };
  }

  return {
    isDocked: false,
    kicker: ui.editorKicker,
    title: ui.editorTitle,
    content: ui.editorContent
  };
}

function setDockPlaceholder() {
  ui.dockEditorKicker.textContent = t("editor.kicker");
  ui.dockEditorTitle.textContent = t("editor.selectPromptTitle");
  ui.dockEditorContent.innerHTML = `
    <div class="editor-card">
      <p class="muted">${t("editor.selectPromptBody")}</p>
    </div>
  `;
}

function getButtonAction(config, hotspot) {
  return config.mapping?.[hotspot.side]?.[hotspot.runtimeId] || "none";
}

function isHotspotConfigured(config, hotspot) {
  if (hotspot.type === "button") {
    return getButtonAction(config, hotspot) !== "none";
  }
  if (hotspot.type === "mouse") {
    const mouseConfig = getMouseConfig(config, hotspot.runtimeId);
    return mouseConfig.enabled !== defaultMouseConfig.enabled
      || mouseConfig.baseSensitivity !== defaultMouseConfig.baseSensitivity
      || mouseConfig.acceleration !== defaultMouseConfig.acceleration
      || mouseConfig.exponent !== defaultMouseConfig.exponent
      || mouseConfig.maxGain !== defaultMouseConfig.maxGain
      || mouseConfig.distanceThreshold !== defaultMouseConfig.distanceThreshold;
  }
  const stickConfig = getStickConfig(config, hotspot.runtimeId);
  return [stickConfig.up, stickConfig.down, stickConfig.left, stickConfig.right].some((value) => value !== "none")
    || stickConfig.deadzone !== defaultStickConfig.deadzone
    || stickConfig.hysteresis !== defaultStickConfig.hysteresis
    || stickConfig.diagonalUnlockRadius !== defaultStickConfig.diagonalUnlockRadius
    || stickConfig.fourWayHysteresisDegrees !== defaultStickConfig.fourWayHysteresisDegrees
    || stickConfig.eightWayHysteresisDegrees !== defaultStickConfig.eightWayHysteresisDegrees;
}

function makeActionBtn(className, text) {
  const element = document.createElement("div");
  element.className = `action-btn ${className} btn-text`;
  element.textContent = text;
  return element;
}

function createHotspotElement(hotspot, config) {
  const button = document.createElement("button");
  const hotspotLabel = getHotspotLabel(hotspot);
  button.type = "button";
  button.className = "hotspot";
  button.dataset.id = hotspot.id;
  button.title = hotspotLabel;
  button.setAttribute("aria-label", hotspotLabel);

  if (hotspot.customClass) {
    button.classList.add(hotspot.customClass);
  } else {
    button.classList.add(`shape-${hotspot.shape}`);
    button.style.left = `${hotspot.x}px`;
    button.style.top = `${hotspot.y}px`;
    button.style.width = `${hotspot.w}px`;
    button.style.height = `${hotspot.h}px`;
  }

  if (hotspot.type === "stick") {
    button.classList.add("hotspot-stick");
  }
  if (hotspot.type === "mouse") {
    button.classList.add("hotspot-mouse");
  }
  if (selectedHotspotId === hotspot.id) {
    button.classList.add("active");
  }
  button.addEventListener("click", () => openEditor(hotspot.id));
  return button;
}

function createJoyCon(side, config) {
  const element = document.createElement("div");
  element.className = `joycon ${side}`;
  element.innerHTML = `
    <div class="shell">
      <div class="color-rail"></div>
      <div class="sl-sr-btn sl"></div>
      <div class="sl-sr-btn sr"></div>
      <div class="btn-lr-arc"></div>
      <div class="btn-zlzr-outer"></div>
      <div class="stick-base"></div>
      <div class="stick-top"></div>
    </div>
  `;

  const shell = element.querySelector(".shell");
  if (side === "left") {
    const minus = document.createElement("div");
    minus.className = "mini-btn minus-btn btn-text";
    minus.textContent = "-";
    shell.appendChild(minus);
    shell.appendChild(makeActionBtn("btn-up", "▲"));
    shell.appendChild(makeActionBtn("btn-left", "◀"));
    shell.appendChild(makeActionBtn("btn-right", "▶"));
    shell.appendChild(makeActionBtn("btn-down", "▼"));
    const capture = document.createElement("div");
    capture.className = "capture-btn";
    shell.appendChild(capture);
  } else {
    const plus = document.createElement("div");
    plus.className = "mini-btn plus-btn btn-text";
    plus.textContent = "+";
    shell.appendChild(plus);
    shell.appendChild(makeActionBtn("btn-x", "X"));
    shell.appendChild(makeActionBtn("btn-y", "Y"));
    shell.appendChild(makeActionBtn("btn-a", "A"));
    shell.appendChild(makeActionBtn("btn-b", "B"));
    const home = document.createElement("div");
    home.className = "mini-btn home-btn btn-text";
    home.innerHTML = "⌂";
    shell.appendChild(home);
    const cButton = document.createElement("div");
    cButton.className = "c-btn btn-text";
    cButton.textContent = "C";
    shell.appendChild(cButton);
  }

  hotspots[side].forEach((hotspot) => {
    shell.appendChild(createHotspotElement(hotspot, config));
  });
  return element;
}

function renderInteractiveMapper(force = false) {
  if (!latestState?.config) {
    return;
  }
  const nextKey = buildVisualRenderKey(latestState.config);
  if (!force && nextKey === lastRenderedVisualKey) {
    return;
  }

  ui.interactiveMapper.innerHTML = "";
  const stage = document.createElement("div");
  stage.className = "joy-stage";
  const row = document.createElement("div");
  row.className = "joy-row";
  row.appendChild(createJoyCon("left", latestState.config));
  row.appendChild(createJoyCon("right", latestState.config));
  stage.appendChild(row);
  ui.interactiveMapper.appendChild(stage);
  lastRenderedVisualKey = nextKey;
}

function createActionEditor(currentValue, onChange) {
  const wrapper = document.createElement("div");
  wrapper.className = "action-editor";

  const select = document.createElement("select");
  const currentIsCustom = currentValue.startsWith("key_custom:");
  actionOptions.forEach((option) => {
    const node = document.createElement("option");
    node.value = option.value;
    node.textContent = getActionLabel(option);
    if ((!currentIsCustom && currentValue === option.value) || (currentIsCustom && option.value === "key_custom")) {
      node.selected = true;
    }
    select.appendChild(node);
  });

  const keyInput = document.createElement("input");
  keyInput.type = "text";
  keyInput.className = "custom-key-input";
  keyInput.placeholder = t("editor.customKeyPlaceholder");
  keyInput.value = currentIsCustom ? currentValue.slice("key_custom:".length) : "";

  select.addEventListener("change", async () => {
    if (select.value === "key_custom") {
      if (keyInput.value.trim()) {
        await onChange(`key_custom:${keyInput.value.trim()}`);
      }
      return;
    }

    keyInput.value = "";
    await onChange(select.value);
  });

  keyInput.addEventListener("change", async () => {
    const trimmed = keyInput.value.trim();
    if (!trimmed) {
      if (select.value === "key_custom") {
        select.value = "none";
        await onChange("none");
      }
      return;
    }

    select.value = "key_custom";
    await onChange(`key_custom:${trimmed}`);
  });

  wrapper.append(select, keyInput);
  return wrapper;
}

function createButtonEditor(hotspot) {
  const card = document.createElement("div");
  card.className = "editor-card";

  const meta = document.createElement("div");
  meta.className = "editor-meta";
  meta.innerHTML = `
    <span class="meta-chip">${t("buttonEditor.mappingId", { runtimeId: hotspot.runtimeId })}</span>
    <span class="meta-chip">${t("buttonEditor.writeRuntime")}</span>
  `;

  const row = document.createElement("div");
  row.className = "mapping-row";
  const label = document.createElement("div");
  label.className = "mapping-label";
  label.textContent = t("buttonEditor.mappingAction");
  const editor = createActionEditor(getButtonAction(latestState.config, hotspot), async (action) => {
    await api("/api/settings/mapping", {
      method: "POST",
      body: {
        side: hotspot.side,
        buttonId: hotspot.runtimeId,
        action
      }
    });
    await refreshState();
  });
  row.append(label, editor);

  card.append(meta, row);
  return {
    kicker: hotspot.side === "left" ? t("buttonEditor.kickerLeft") : t("buttonEditor.kickerRight"),
    title: getHotspotLabel(hotspot),
    nodes: [card]
  };
}

async function updateStickConfig(side, patch) {
  const nextConfig = {
    ...getStickConfig(latestState.config, side),
    ...patch
  };
  nextConfig.deadzone = Math.max(0, Math.min(32767, Math.round(Number(nextConfig.deadzone) || 0)));
  nextConfig.hysteresis = Math.max(0, Math.min(32767, Math.round(Number(nextConfig.hysteresis) || 0)));
  nextConfig.diagonalUnlockRadius = Math.max(
    nextConfig.deadzone,
    Math.min(32767, Math.round(Number(nextConfig.diagonalUnlockRadius) || 0))
  );
  nextConfig.fourWayHysteresisDegrees = Math.max(0, Math.min(45, Number(nextConfig.fourWayHysteresisDegrees) || 0));
  nextConfig.eightWayHysteresisDegrees = Math.max(0, Math.min(22.5, Number(nextConfig.eightWayHysteresisDegrees) || 0));

  await api("/api/settings/stick", {
    method: "POST",
    body: {
      side,
      ...nextConfig
    }
  });
  await refreshState();
}

function createStickEditor(hotspot) {
  const stickConfig = getStickConfig(latestState.config, hotspot.runtimeId);

  const infoCard = document.createElement("div");
  infoCard.className = "editor-card";
  infoCard.innerHTML = `
    <p class="muted">${t("stickEditor.rangeHint")}</p>
    <div class="editor-meta">
      <span class="meta-chip">${t("stickEditor.radialHysteresisTag")}</span>
      <span class="meta-chip">${t("stickEditor.angularHysteresisTag")}</span>
      <span class="meta-chip">${t("stickEditor.diagonalUnlockTag")}</span>
    </div>
  `;

  const card = document.createElement("div");
  card.className = "editor-card stick-grid";

  const rangeFields = [
    { labelKey: "stickEditor.deadzone", key: "deadzone", min: "0", max: "32767", step: "256" },
    { labelKey: "stickEditor.hysteresis", key: "hysteresis", min: "0", max: "8192", step: "128" },
    { labelKey: "stickEditor.diagonalUnlockRadius", key: "diagonalUnlockRadius", min: String(stickConfig.deadzone), max: "32767", step: "256" },
    { labelKey: "stickEditor.fourWayHysteresisDegrees", key: "fourWayHysteresisDegrees", min: "0", max: "45", step: "1" },
    { labelKey: "stickEditor.eightWayHysteresisDegrees", key: "eightWayHysteresisDegrees", min: "0", max: "22.5", step: "0.5" }
  ];

  rangeFields.forEach((field) => {
    const label = document.createElement("label");
    label.textContent = t(field.labelKey);

    const input = document.createElement("input");
    input.type = "range";
    input.min = field.min;
    input.max = field.max;
    input.step = field.step;
    input.value = String(stickConfig[field.key]);

    const value = document.createElement("span");
    value.textContent = input.value;

    input.addEventListener("input", () => {
      value.textContent = input.value;
    });
    input.addEventListener("change", async () => {
      await updateStickConfig(hotspot.runtimeId, { [field.key]: Number(input.value) });
    });

    label.append(input, value);
    card.appendChild(label);
  });

  stickDirectionEntries.forEach((entry) => {
    const row = document.createElement("div");
    row.className = "mapping-row";
    const label = document.createElement("div");
    label.className = "mapping-label";
    label.textContent = t(entry.labelKey);
    const editor = createActionEditor(stickConfig[entry.id] || "none", async (action) => {
      await updateStickConfig(hotspot.runtimeId, { [entry.id]: action });
    });
    row.append(label, editor);
    card.appendChild(row);
  });

  return {
    kicker: hotspot.runtimeId === "left" ? t("stickEditor.kickerLeft") : t("stickEditor.kickerRight"),
    title: getHotspotLabel(hotspot),
    nodes: [infoCard, card]
  };
}

function makeMouseDraftPatch(side, patch) {
  mouseDraftConfig[side] = {
    ...getEffectiveMouseConfig(side),
    ...patch
  };
  mouseDraftDirty[side] = true;
  if (latestState?.config) {
    latestState.config.mouse[side] = { ...mouseDraftConfig[side] };
    lastRenderedPopupKey = buildPopupRenderKey(latestState.config);
  }
}

function makeServerDraftPatch(patch) {
  serverDraftConfig = {
    ...getEffectiveServerConfig(),
    ...patch
  };
  serverDraftDirty = true;
  if (latestState?.config) {
    latestState.config.server = { ...serverDraftConfig };
    lastRenderedPopupKey = buildPopupRenderKey(latestState.config);
  }
}

function createMouseControl(labelText, input, valueNode) {
  const label = document.createElement("label");
  label.textContent = labelText;
  if (valueNode) {
    label.append(input, valueNode);
  } else {
    label.append(input);
  }
  return label;
}

function createMouseEditor(hotspot) {
  const mouseConfig = getEffectiveMouseConfig(hotspot.runtimeId);
  const isLeft = hotspot.runtimeId === "left";

  const infoCard = document.createElement("div");
  infoCard.className = "editor-card";
  infoCard.innerHTML = `
    <p class="muted">${t("mouseEditor.draftHint")}</p>
    <div class="editor-meta">
      <span class="meta-chip">${t(isLeft ? "mouseEditor.opticalTagLeft" : "mouseEditor.opticalTagRight")}</span>
      <span class="meta-chip">${t("mouseEditor.sharedEditorTag")}</span>
    </div>
  `;

  const card = document.createElement("div");
  card.className = "editor-card";
  const grid = document.createElement("div");
  grid.className = "control-grid";

  const enabledInput = document.createElement("input");
  enabledInput.type = "checkbox";
  enabledInput.checked = mouseConfig.enabled;
  enabledInput.addEventListener("change", () => {
    makeMouseDraftPatch(hotspot.runtimeId, { enabled: enabledInput.checked });
  });
  grid.appendChild(createMouseControl(t("mouseEditor.enableMapping"), enabledInput));

  const rangeFields = [
    { labelKey: "mouseEditor.baseSensitivity", key: "baseSensitivity", min: "0.02", max: "1.00", step: "0.01", format: (value) => Number(value).toFixed(2) },
    { labelKey: "mouseEditor.acceleration", key: "acceleration", min: "0.00", max: "0.20", step: "0.005", format: (value) => Number(value).toFixed(3) },
    { labelKey: "mouseEditor.exponent", key: "exponent", min: "0.20", max: "1.50", step: "0.01", format: (value) => Number(value).toFixed(2) },
    { labelKey: "mouseEditor.maxGain", key: "maxGain", min: "0.50", max: "8.00", step: "0.10", format: (value) => Number(value).toFixed(2) },
    { labelKey: "mouseEditor.distanceThreshold", key: "distanceThreshold", min: "0", max: "12", step: "1", format: (value) => formatDistanceThreshold(value) }
  ];

  rangeFields.forEach((field) => {
    const input = document.createElement("input");
    input.type = "range";
    input.min = field.min;
    input.max = field.max;
    input.step = field.step;
    input.value = String(mouseConfig[field.key]);

    const value = document.createElement("span");
    value.textContent = field.format(input.value);

    input.addEventListener("input", () => {
      value.textContent = field.format(input.value);
      makeMouseDraftPatch(hotspot.runtimeId, { [field.key]: Number(input.value) });
    });

    input.addEventListener("change", () => {
      value.textContent = field.format(input.value);
      makeMouseDraftPatch(hotspot.runtimeId, { [field.key]: Number(input.value) });
    });

    grid.appendChild(createMouseControl(t(field.labelKey), input, value));
  });

  card.appendChild(grid);
  return {
    kicker: t(isLeft ? "mouseEditor.kickerLeft" : "mouseEditor.kickerRight"),
    title: getHotspotLabel(hotspot),
    nodes: [infoCard, card]
  };
}

function createServerEditor() {
  const serverConfig = getEffectiveServerConfig();

  const infoCard = document.createElement("div");
  infoCard.className = "editor-card";
  infoCard.innerHTML = `
    <p class="muted">${t("serverEditor.draftHint")}</p>
    <div class="editor-meta">
      <span class="meta-chip">${t("serverEditor.localTag")}</span>
      <span class="meta-chip">${t("serverEditor.sharedEditorTag")}</span>
    </div>
  `;

  const card = document.createElement("div");
  card.className = "editor-card";
  const grid = document.createElement("div");
  grid.className = "control-grid";

  const portInput = document.createElement("input");
  portInput.type = "number";
  portInput.className = "custom-key-input";
  portInput.min = "1";
  portInput.max = "65535";
  portInput.step = "1";
  portInput.value = String(serverConfig.port);
  portInput.addEventListener("change", () => {
    const nextPort = normalizePort(portInput.value);
    portInput.value = String(nextPort);
    makeServerDraftPatch({ port: nextPort });
  });
  grid.appendChild(createMouseControl(t("serverEditor.webPort"), portInput));

  card.appendChild(grid);
  return {
    kicker: t("serverEditor.kicker"),
    title: t("serverEditor.title"),
    nodes: [infoCard, card]
  };
}

function updateLocalizedStateText(forceConfigRender = false) {
  if (!latestState) {
    setDockPlaceholder();
    return;
  }

  ui.leftStatus.textContent = translateStatusValue(latestState.left.status);
  ui.rightStatus.textContent = translateStatusValue(latestState.right.status);
  applyStatusPillState(ui.leftStatus, latestState.left.status);
  applyStatusPillState(ui.rightStatus, latestState.right.status);
  ui.leftName.textContent = latestState.left.deviceName || t("device.notConnected");
  ui.rightName.textContent = latestState.right.deviceName || t("device.notConnected");
  ui.leftStats.textContent = formatControllerStats(latestState.left);
  ui.rightStats.textContent = formatControllerStats(latestState.right);
  ui.mouseStats.textContent = formatMouseStats(latestState.mouseStats);
  if (forceConfigRender) {
    lastRenderedVisualKey = "";
    lastRenderedPopupKey = "";
  }
  renderInteractiveMapper(forceConfigRender);
  renderEditor(forceConfigRender);
}

function createEditorDescriptor(hotspot) {
  if (!hotspot) {
    return null;
  }
  if (hotspot.type === "button") {
    return createButtonEditor(hotspot);
  }
  if (hotspot.type === "stick") {
    return createStickEditor(hotspot);
  }
  if (hotspot.type === "mouse") {
    return createMouseEditor(hotspot);
  }
  if (hotspot.type === "server") {
    return createServerEditor();
  }
  return null;
}

function renderEditor(force = false) {
  if (!latestState?.config) {
    return;
  }

  const editorUi = getActiveEditorUi();

  const hotspot = findHotspot();
  const descriptor = createEditorDescriptor(hotspot);
  if (!descriptor) {
    ui.editorModal.classList.add("hidden");
    ui.editorModal.setAttribute("aria-hidden", "true");
    setDockPlaceholder();
    lastRenderedPopupKey = "";
    return;
  }

  const nextKey = buildPopupRenderKey(latestState.config);
  if (!force && nextKey === lastRenderedPopupKey) {
    return;
  }

  if (editorUi.isDocked) {
    ui.editorModal.classList.add("hidden");
    ui.editorModal.setAttribute("aria-hidden", "true");
  } else {
    ui.editorModal.classList.remove("hidden");
    ui.editorModal.setAttribute("aria-hidden", "false");
  }
  editorUi.kicker.textContent = descriptor.kicker;
  editorUi.title.textContent = descriptor.title;
  editorUi.content.innerHTML = "";
  descriptor.nodes.forEach((node) => {
    editorUi.content.appendChild(node);
  });

  lastRenderedPopupKey = nextKey;
}

function openEditor(hotspotId) {
  selectedHotspotId = hotspotId;
  renderInteractiveMapper(true);
  renderEditor(true);
}

function closeEditor() {
  selectedHotspotId = null;
  renderInteractiveMapper(true);
  renderEditor(true);
}

function renderState(snapshot) {
  const normalizedConfig = normalizeConfig(snapshot.config || {});
  if (mouseDraftDirty.left && mouseDraftConfig.left) {
    normalizedConfig.mouse.left = getEffectiveMouseConfig("left");
  }
  if (mouseDraftDirty.right && mouseDraftConfig.right) {
    normalizedConfig.mouse.right = getEffectiveMouseConfig("right");
  }
  if (serverDraftDirty && serverDraftConfig) {
    normalizedConfig.server = getEffectiveServerConfig();
  }
  latestState = {
    ...snapshot,
    config: normalizedConfig
  };

  updateLocalizedStateText();
}

async function refreshState() {
  if (redirectInFlight) {
    return;
  }
  const state = await api("/api/state");
  renderState(state);
}

function bindActions() {
  ui.languageSelect.addEventListener("change", () => {
    setLanguage(ui.languageSelect.value);
  });

  ui.serverSettingsBtn.addEventListener("click", () => {
    openEditor("server-settings");
  });

  document.getElementById("connectLeftBtn").addEventListener("click", async () => {
    showToast(t("prompt.holdSyncToConnect"));
    await api("/api/connect/left", { method: "POST" });
    await refreshState();
  });

  document.getElementById("connectRightBtn").addEventListener("click", async () => {
    showToast(t("prompt.holdSyncToConnect"));
    await api("/api/connect/right", { method: "POST" });
    await refreshState();
  });

  document.getElementById("disconnectLeftBtn").addEventListener("click", async () => {
    await api("/api/disconnect/left", { method: "POST" });
    await refreshState();
  });

  document.getElementById("disconnectRightBtn").addEventListener("click", async () => {
    await api("/api/disconnect/right", { method: "POST" });
    await refreshState();
  });

  ui.saveConfigBtn.addEventListener("click", async () => {
    const result = await saveAndApplyBrowserConfig(buildBrowserConfig());
    mouseDraftDirty = { left: false, right: false };
    mouseDraftConfig = { left: null, right: null };
    serverDraftDirty = false;
    serverDraftConfig = null;
    if (!result.redirected) {
      await refreshState();
    }
  });

  ui.downloadConfigBtn.addEventListener("click", async () => {
    const config = buildBrowserConfig();
    const blob = new Blob([JSON.stringify(config, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const link = document.createElement("a");
    link.href = url;
    link.download = "config.json";
    link.click();
    URL.revokeObjectURL(url);
  });

  ui.loadConfigBtn.addEventListener("click", () => {
    ui.importConfigInput.click();
  });

  ui.importConfigInput.addEventListener("change", async (event) => {
    const file = event.target.files?.[0];
    if (!file) {
      return;
    }

    try {
      const text = await file.text();
      const parsed = JSON.parse(text);
      applyConfigToUi(parsed);
      const result = await saveAndApplyBrowserConfig(buildBrowserConfig());
      if (!result.redirected) {
        await refreshState();
      }
    } finally {
      event.target.value = "";
    }
  });

  ui.editorBackdrop.addEventListener("click", closeEditor);
  ui.closeEditorBtn.addEventListener("click", closeEditor);
  dockedEditorMedia.addEventListener("change", () => {
    lastRenderedPopupKey = "";
    renderEditor(true);
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && selectedHotspotId) {
      closeEditor();
    }
  });
}

async function boot() {
  applyStaticTranslations();
  onLanguageChange(() => {
    applyStaticTranslations();
    updateLocalizedStateText(true);
  });
  bindActions();
  await refreshState();
  refreshTimer = setInterval(refreshState, 500);
}

boot();
