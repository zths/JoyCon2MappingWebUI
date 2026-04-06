(() => {
  const STORAGE_KEY = "joycon-webgui-language";
  const translations = {
    "zh-CN": {
      meta: {
        title: "Joy-Con 2 Web GUI"
      },
      app: {
        tagline: "连接手柄、调整鼠标参数、编辑按键与摇杆映射。"
      },
      language: {
        label: "语言"
      },
      toolbar: {
        serverSettings: "服务设置",
        saveApply: "保存并应用",
        export: "导出",
        import: "导入"
      },
      prompt: {
        holdSyncToConnect: "长按 Sync 键(侧面小圆钮)来连接",
        redirectingToNewPort: "端口已更新，正在跳转到 {port}"
      },
      device: {
        left: "左 Joy-Con 2",
        right: "右 Joy-Con 2",
        notConnected: "未连接"
      },
      button: {
        connect: "连接",
        disconnect: "断开"
      },
      section: {
        interactiveMapping: "交互式映射",
        interactiveMappingHint: "点击手柄上的按钮、摇杆或鼠标区域，即可在右侧编辑对应的映射设置。",
        realtimeStatus: "实时状态"
      },
      side: {
        left: "左侧",
        right: "右侧",
        mouse: "鼠标"
      },
      editor: {
        kicker: "映射编辑",
        selectPromptTitle: "选择一个按键",
        selectPromptBody: "点击左侧手柄上的按钮、摇杆或鼠标入口后，在这里编辑对应设置。",
        close: "关闭",
        closeDialog: "关闭浮窗",
        customKeyPlaceholder: "例如 W / Space / Esc"
      },
      buttonEditor: {
        mappingId: "映射 ID: {runtimeId}",
        mappingAction: "映射动作",
        kickerLeft: "左 Joy-Con 2 按键",
        kickerRight: "右 Joy-Con 2 按键"
      },
      stickEditor: {
        rangeHint: "摇杆范围：左 <code>-32767</code> / 中 <code>0</code> / 右 <code>32767</code>，上 <code>-32767</code> / 中 <code>0</code> / 下 <code>32767</code>。",
        deadzone: "死区",
        hysteresis: "径向粘滞",
        diagonalUnlockRadius: "8 向解锁半径",
        fourWayHysteresisDegrees: "4 向角度粘滞",
        eightWayHysteresisDegrees: "8 向角度粘滞",
        radialHysteresisTag: "按下/松开分离阈值",
        angularHysteresisTag: "扇区切换角度粘滞",
        diagonalUnlockTag: "近区 4 向，远区 8 向",
        stickPress: "摇杆按下（{id}）",
        kickerLeft: "左摇杆设置",
        kickerRight: "右摇杆设置"
      },
      mouseEditor: {
        opticalTagLeft: "左侧 Joy-Con 2 光学鼠标",
        opticalTagRight: "右侧 Joy-Con 2 光学鼠标",
        enableMapping: "启用鼠标映射",
        baseSensitivity: "基础灵敏度",
        acceleration: "加速度",
        exponent: "指数",
        maxGain: "最大增益",
        distanceThreshold: "光学距离阈值",
        kickerLeft: "左鼠标设置",
        kickerRight: "右鼠标设置"
      },
      serverEditor: {
        localTag: "本地 Web 服务",
        webPort: "Web UI 端口",
        kicker: "服务设置",
        title: "服务设置"
      },
      directions: {
        up: "上",
        left: "左",
        down: "下",
        right: "右"
      },
      actions: {
        none: "无",
        mouseLeft: "鼠标左键",
        mouseRight: "鼠标右键",
        mouseMiddle: "鼠标中键",
        keySpace: "空格键",
        keyEnter: "回车键",
        keyEscape: "Esc 键",
        keyTab: "Tab 键",
        keyCtrl: "Ctrl 键",
        keyShift: "Shift 键",
        keyAlt: "Alt 键",
        keyUp: "方向上",
        keyDown: "方向下",
        keyLeft: "方向左",
        keyRight: "方向右",
        keyW: "W 键",
        keyA: "A 键",
        keyS: "S 键",
        keyD: "D 键",
        keyQ: "Q 键",
        keyE: "E 键",
        keyR: "R 键",
        keyF: "F 键",
        key1: "1 键",
        key2: "2 键",
        key3: "3 键",
        key4: "4 键",
        key5: "5 键",
        keyCustom: "自定义按键..."
      },
      hotspots: {
        leftZl: "ZL 键",
        leftL: "L 键",
        leftMinus: "- 键",
        leftStick: "左摇杆",
        leftUp: "上方向键",
        leftLeft: "左方向键",
        leftRight: "右方向键",
        leftDown: "下方向键",
        leftCapture: "截图键",
        leftMouse: "左鼠标",
        leftSl: "SL 键",
        leftSr: "SR 键",
        rightZr: "ZR 键",
        rightR: "R 键",
        rightMouse: "鼠标设置",
        rightPlus: "+ 键",
        rightX: "X 键",
        rightY: "Y 键",
        rightA: "A 键",
        rightB: "B 键",
        rightStick: "右摇杆",
        rightHome: "HOME 键",
        rightC: "C 键",
        rightSl: "SL 键",
        rightSr: "SR 键"
      },
      stats: {
        controller: {
          status: "状态",
          device: "设备",
          packets: "数据包",
          rateHz: "频率(Hz)",
          avgIntervalMs: "平均间隔(ms)",
          buttonBits: "按键位",
          opticalDistance: "光学距离",
          optical: "光学坐标",
          leftStick: "左摇杆",
          rightStick: "右摇杆",
          error: "错误"
        },
        mouse: {
          movedPackets: "移动包",
          injectedMoves: "注入移动",
          gatedPackets: "过滤包",
          avgDispatchUs: "平均派发(us)",
          maxDispatchUs: "最大派发(us)",
          distanceLast: "最近距离",
          distanceMin: "最小距离",
          distanceMax: "最大距离"
        }
      },
      statusValue: {
        connected: "已连接",
        disconnected: "未连接",
        connecting: "连接中",
        error: "错误"
      }
    },
    en: {
      meta: {
        title: "Joy-Con 2 Web GUI"
      },
      app: {
        tagline: "Connect controllers, tune mouse parameters, and edit button and stick mappings."
      },
      language: {
        label: "Language"
      },
      toolbar: {
        serverSettings: "Server Settings",
        saveApply: "Save & Apply",
        export: "Export",
        import: "Import"
      },
      prompt: {
        holdSyncToConnect: "Hold the Sync button (small side button) to connect",
        redirectingToNewPort: "Port updated. Redirecting to {port}"
      },
      device: {
        left: "Left Joy-Con 2",
        right: "Right Joy-Con 2",
        notConnected: "Not connected"
      },
      button: {
        connect: "Connect",
        disconnect: "Disconnect"
      },
      section: {
        interactiveMapping: "Interactive Mapping",
        interactiveMappingHint: "Click a button, stick, or mouse zone on the controller to edit its mapping on the right.",
        realtimeStatus: "Live Status"
      },
      side: {
        left: "Left",
        right: "Right",
        mouse: "Mouse"
      },
      editor: {
        kicker: "Mapping Editor",
        selectPromptTitle: "Select a control",
        selectPromptBody: "Click a button, stick, or mouse entry on the controller to edit its settings here.",
        close: "Close",
        closeDialog: "Close dialog",
        customKeyPlaceholder: "For example: W / Space / Esc"
      },
      buttonEditor: {
        mappingId: "Mapping ID: {runtimeId}",
        mappingAction: "Mapped action",
        kickerLeft: "Left Joy-Con 2 Button",
        kickerRight: "Right Joy-Con 2 Button"
      },
      stickEditor: {
        rangeHint: "Stick range: left <code>-32767</code> / center <code>0</code> / right <code>32767</code>, up <code>-32767</code> / center <code>0</code> / down <code>32767</code>.",
        deadzone: "Deadzone",
        hysteresis: "Radial hysteresis",
        diagonalUnlockRadius: "Diagonal unlock radius",
        fourWayHysteresisDegrees: "4-way angular hysteresis",
        eightWayHysteresisDegrees: "8-way angular hysteresis",
        radialHysteresisTag: "Separate press/release thresholds",
        angularHysteresisTag: "Angular hysteresis between sectors",
        diagonalUnlockTag: "Cardinal near center, diagonal farther out",
        stickPress: "Stick press ({id})",
        kickerLeft: "Left Stick Settings",
        kickerRight: "Right Stick Settings"
      },
      mouseEditor: {
        opticalTagLeft: "Left Joy-Con 2 optical mouse",
        opticalTagRight: "Right Joy-Con 2 optical mouse",
        enableMapping: "Enable mouse mapping",
        baseSensitivity: "Base sensitivity",
        acceleration: "Acceleration",
        exponent: "Exponent",
        maxGain: "Max gain",
        distanceThreshold: "Optical distance threshold",
        kickerLeft: "Left Mouse Settings",
        kickerRight: "Right Mouse Settings"
      },
      serverEditor: {
        localTag: "Local Web server",
        webPort: "Web UI port",
        kicker: "Server Settings",
        title: "Server Settings"
      },
      directions: {
        up: "Up",
        left: "Left",
        down: "Down",
        right: "Right"
      },
      actions: {
        none: "None",
        mouseLeft: "Mouse Left",
        mouseRight: "Mouse Right",
        mouseMiddle: "Mouse Middle",
        keySpace: "Key Space",
        keyEnter: "Key Enter",
        keyEscape: "Key Escape",
        keyTab: "Key Tab",
        keyCtrl: "Key Ctrl",
        keyShift: "Key Shift",
        keyAlt: "Key Alt",
        keyUp: "Key Up",
        keyDown: "Key Down",
        keyLeft: "Key Left",
        keyRight: "Key Right",
        keyW: "Key W",
        keyA: "Key A",
        keyS: "Key S",
        keyD: "Key D",
        keyQ: "Key Q",
        keyE: "Key E",
        keyR: "Key R",
        keyF: "Key F",
        key1: "Key 1",
        key2: "Key 2",
        key3: "Key 3",
        key4: "Key 4",
        key5: "Key 5",
        keyCustom: "Custom Key..."
      },
      hotspots: {
        leftZl: "ZL Button",
        leftL: "L Button",
        leftMinus: "- Button",
        leftStick: "Left Stick",
        leftUp: "Up Button",
        leftLeft: "Left Button",
        leftRight: "Right Button",
        leftDown: "Down Button",
        leftCapture: "Capture Button",
        leftMouse: "Left Mouse",
        leftSl: "SL Button",
        leftSr: "SR Button",
        rightZr: "ZR Button",
        rightR: "R Button",
        rightMouse: "Mouse Settings",
        rightPlus: "+ Button",
        rightX: "X Button",
        rightY: "Y Button",
        rightA: "A Button",
        rightB: "B Button",
        rightStick: "Right Stick",
        rightHome: "HOME Button",
        rightC: "C Button",
        rightSl: "SL Button",
        rightSr: "SR Button"
      },
      stats: {
        controller: {
          status: "status",
          device: "device",
          packets: "packets",
          rateHz: "rate_hz",
          avgIntervalMs: "avg_interval_ms",
          buttonBits: "button_bits",
          opticalDistance: "optical_distance",
          optical: "optical",
          leftStick: "left_stick",
          rightStick: "right_stick",
          error: "error"
        },
        mouse: {
          movedPackets: "moved_packets",
          injectedMoves: "injected_moves",
          gatedPackets: "gated_packets",
          avgDispatchUs: "avg_dispatch_us",
          maxDispatchUs: "max_dispatch_us",
          distanceLast: "distance_last",
          distanceMin: "distance_min",
          distanceMax: "distance_max"
        }
      },
      statusValue: {
        connected: "connected",
        disconnected: "disconnected",
        connecting: "connecting",
        error: "error"
      }
    }
  };

  function normalizeLanguage(language) {
    const lower = String(language || "").toLowerCase();
    if (lower.startsWith("zh")) {
      return "zh-CN";
    }
    return "en";
  }

  function lookup(dictionary, key) {
    return key.split(".").reduce((value, part) => value?.[part], dictionary);
  }

  function interpolate(template, params) {
    return template.replace(/\{(\w+)\}/g, (_, key) => String(params[key] ?? ""));
  }

  let currentLanguage = normalizeLanguage(localStorage.getItem(STORAGE_KEY) || navigator.language || "zh-CN");
  const listeners = new Set();

  function t(key, params = {}, fallback = key) {
    const text = lookup(translations[currentLanguage], key) ?? lookup(translations.en, key) ?? fallback;
    return typeof text === "string" ? interpolate(text, params) : fallback;
  }

  function setLanguage(language) {
    const nextLanguage = normalizeLanguage(language);
    if (nextLanguage === currentLanguage) {
      return currentLanguage;
    }
    currentLanguage = nextLanguage;
    localStorage.setItem(STORAGE_KEY, currentLanguage);
    listeners.forEach((listener) => listener(currentLanguage));
    return currentLanguage;
  }

  function subscribe(listener) {
    listeners.add(listener);
    return () => listeners.delete(listener);
  }

  window.I18n = {
    t,
    setLanguage,
    subscribe,
    getLanguage: () => currentLanguage,
    getSupportedLanguages: () => ["zh-CN", "en"]
  };
})();
