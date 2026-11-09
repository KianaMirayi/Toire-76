/* =====================================================================
   ui-config.js —— renders/components.json 的内联副本（自动生成，别手改）

   为什么内联：WebView2 用 file:// 打开页面时，XHR/JSON 读取受同源策略
   限制，读相邻目录的 components.json 会失败 -> 整个 UI 白屏。
   所以构建期把 JSON 直接写成 window.TOIRE76_CFG。

   来源：renders/components.json（Blender 管线产出）
   ===================================================================== */
window.TOIRE76_CFG ={
 "schema": "torei76.components/1",
 "generated_by": "blender/tools/build_components_json.py",
 "source_spec": "blender/spec.py",
 "canvas": {
  "w": 950,
  "h": 175,
  "scale": 2,
  "mm_per_px": 0.508,
  "px_per_mm": 1.968504,
  "panel_mm": [
   482.6,
   88.9
  ],
  "origin": "top-left, y down (image convention)"
 },
 "angle_convention": {
  "theta_cw_deg": "以 12 点方向为 0°，顺时针为正；= 图像角 + 90°（图像角以 +x 向右、y 向下为基准）",
  "css_rotate": "CSS rotate(θ) 正角=顺时针，与本约定同向；transform-origin 用各资产的 pivot_in_image_css",
  "knobs": "angle_min/max 均为 theta_cw；零点=正上方（已实测 knob_pointer 质心方向 theta_cw=0.00°、knob_dot=0.17°）",
  "needle": "meter_needle_single.angle_min/max = -49.13 / 51.16 是 theta_cw；对应图像角 -139.13 / -38.84（+90 换算一致）"
 },
 "layers_back_to_front": [
  "PANEL_Faceplate",
  "PANEL_Chassis(仅3D)",
  "SCREW_*",
  "BTN_Ratio_* / SW_Meter",
  "KNOB_*",
  "METER_Face",
  "METER_Needle",
  "METER_Glass",
  "METER_Bezel"
 ],
 "components": [
  {
   "id": "knob_input",
   "obj": "KNOB_Input_Body",
   "kind": "rotary",
   "box": [
    91,
    42,
    90,
    90
   ],
   "pivot_px": [
    136,
    87
   ],
   "pivot_space": "ref_canvas_950x175",
   "center_mm": [
    -172.212,
    0.254
   ],
   "diameter_mm": 45.72,
   "body_height_mm": 3.0,
   "body_note": "黑滚花身 + 抛光铝帽（见 cap/grip；原厂 sprite 基准）",
   "cap": {
    "diameter_mm": 19.3,
    "height_mm": 1.35,
    "material": "polished_aluminum(metalness 0.5, roughness 0.2, 车削纹)",
    "render_rgb_target": [
     200,
     215
    ],
    "note": "抛光铝顶帽（sprite 实测；非黑坑）"
   },
   "grip": {
    "diameter_mm": 24.4,
    "height_mm": 3.0,
    "knurl_teeth": 38,
    "knurl_depth_mm": 0.5,
    "render_rgb_target": [
     60,
     100
    ]
   },
   "indicator": {
    "type": "white_line",
    "width_mm": 0.9,
    "r0_mm": 9.3,
    "r1_mm": 20.1,
    "color": "#FFFFFF",
    "note": "白细线，直角端头"
   },
   "shadow": {
    "asset_big": "knob_shadow_big.png",
    "asset_small": "knob_shadow_small.png",
    "diameter_mm": 42.68,
    "alpha_center": 150,
    "alpha_edge": 0,
    "offset_mm": [
     0.0,
     0.0
    ],
    "note": "半透明投影盘（原厂 sprite 自带；不是实体）"
   },
   "angle_min_deg": 159.9,
   "angle_max_deg": -161.52,
   "sweep_deg": 321.42,
   "direction": "ccw_increase",
   "angle_convention": "theta_cw_deg_from_12oclock_clockwise",
   "param": "input",
   "value_range": [
    0,
    1
   ],
   "default_value": 0.42,
   "frames": 61,
   "frame_w_px": 180,
   "frame_h_px": 180,
   "frame_w_css": 90,
   "frame_h_css": 90,
   "strip": "knob_input_61f.png",
   "pivot_in_frame_px": [
    90,
    90
   ],
   "pivot_in_frame_css": [
    45,
    45
   ],
   "alpha_bbox_frame_px": [
    6,
    6,
    173,
    173
   ],
   "parts": {
    "body": "KNOB_Input_Body",
    "cap": "KNOB_Input_Cap",
    "indicator": "KNOB_Input_Pointer"
   },
   "part_notes": "body 为父（旋转 pivot 在轴心），cap / indicator 为子对象（原点在帽中心）",
   "filmstrip_deprecated": true,
   "sprite_mode": "body + indicator 分离（见 knob_body_* / knob_pointer / knob_dot）"
  },
  {
   "id": "knob_output",
   "obj": "KNOB_Output_Body",
   "kind": "rotary",
   "box": [
    278,
    42,
    90,
    90
   ],
   "pivot_px": [
    323,
    87
   ],
   "pivot_space": "ref_canvas_950x175",
   "center_mm": [
    -77.216,
    0.254
   ],
   "diameter_mm": 45.72,
   "body_height_mm": 3.0,
   "body_note": "黑滚花身 + 抛光铝帽（见 cap/grip；原厂 sprite 基准）",
   "cap": {
    "diameter_mm": 19.3,
    "height_mm": 1.35,
    "material": "polished_aluminum(metalness 0.5, roughness 0.2, 车削纹)",
    "render_rgb_target": [
     200,
     215
    ],
    "note": "抛光铝顶帽（sprite 实测；非黑坑）"
   },
   "grip": {
    "diameter_mm": 24.4,
    "height_mm": 3.0,
    "knurl_teeth": 38,
    "knurl_depth_mm": 0.5,
    "render_rgb_target": [
     60,
     100
    ]
   },
   "indicator": {
    "type": "white_line",
    "width_mm": 0.9,
    "r0_mm": 9.3,
    "r1_mm": 20.1,
    "color": "#FFFFFF",
    "note": "白细线，直角端头"
   },
   "shadow": {
    "asset_big": "knob_shadow_big.png",
    "asset_small": "knob_shadow_small.png",
    "diameter_mm": 42.68,
    "alpha_center": 150,
    "alpha_edge": 0,
    "offset_mm": [
     0.0,
     0.0
    ],
    "note": "半透明投影盘（原厂 sprite 自带；不是实体）"
   },
   "angle_min_deg": 159.9,
   "angle_max_deg": -161.52,
   "sweep_deg": 321.42,
   "direction": "ccw_increase",
   "angle_convention": "theta_cw_deg_from_12oclock_clockwise",
   "param": "output",
   "value_range": [
    0,
    1
   ],
   "default_value": 0.33,
   "frames": 61,
   "frame_w_px": 180,
   "frame_h_px": 180,
   "frame_w_css": 90,
   "frame_h_css": 90,
   "strip": "knob_input_61f.png",
   "pivot_in_frame_px": [
    90,
    90
   ],
   "pivot_in_frame_css": [
    45,
    45
   ],
   "alpha_bbox_frame_px": [
    6,
    6,
    173,
    173
   ],
   "parts": {
    "body": "KNOB_Output_Body",
    "cap": "KNOB_Output_Cap",
    "indicator": "KNOB_Output_Pointer"
   },
   "part_notes": "body 为父（旋转 pivot 在轴心），cap / indicator 为子对象（原点在帽中心）",
   "filmstrip_deprecated": true,
   "sprite_mode": "body + indicator 分离（见 knob_body_* / knob_pointer / knob_dot）"
  },
  {
   "id": "knob_attack",
   "obj": "KNOB_Attack_Body",
   "kind": "rotary",
   "box": [
    445,
    19,
    60,
    60
   ],
   "pivot_px": [
    475,
    49
   ],
   "pivot_space": "ref_canvas_950x175",
   "center_mm": [
    0.0,
    19.558
   ],
   "diameter_mm": 30.48,
   "body_height_mm": 2.8,
   "body_note": "黑滚花身 + 抛光铝帽（见 cap/grip；原厂 sprite 基准）",
   "cap": {
    "diameter_mm": 13.2,
    "height_mm": 1.15,
    "material": "polished_aluminum(metalness 0.5, roughness 0.2, 车削纹)",
    "render_rgb_target": [
     200,
     215
    ],
    "note": "抛光铝顶帽（sprite 实测；非黑坑）"
   },
   "grip": {
    "diameter_mm": 19.3,
    "height_mm": 2.8,
    "knurl_teeth": 29,
    "knurl_depth_mm": 0.42,
    "render_rgb_target": [
     60,
     100
    ]
   },
   "indicator": {
    "type": "dark_dot",
    "diameter_mm": 2.5,
    "center_radius_mm": 5.1,
    "color": "#0A0A0C",
    "note": "帽面小黑点（sprite 实测，非白线）"
   },
   "shadow": {
    "asset_big": "knob_shadow_big.png",
    "asset_small": "knob_shadow_small.png",
    "diameter_mm": 20.0,
    "alpha_center": 150,
    "alpha_edge": 0,
    "offset_mm": [
     -1.2,
     1.4
    ],
    "note": "半透明投影盘（原厂 sprite 自带；不是实体）"
   },
   "angle_min_deg": 210.8,
   "angle_max_deg": 509.7,
   "sweep_deg": 298.9,
   "direction": "cw_increase",
   "angle_convention": "theta_cw_deg_from_12oclock_clockwise",
   "param": "attack",
   "value_range": [
    0,
    1
   ],
   "default_value": 0.45,
   "frames": 61,
   "frame_w_px": 120,
   "frame_h_px": 120,
   "frame_w_css": 60,
   "frame_h_css": 60,
   "strip": "knob_attack_61f.png",
   "pivot_in_frame_px": [
    60,
    60
   ],
   "pivot_in_frame_css": [
    30,
    30
   ],
   "alpha_bbox_frame_px": [
    16,
    15,
    98,
    98
   ],
   "parts": {
    "body": "KNOB_Attack_Body",
    "cap": "KNOB_Attack_Cap",
    "indicator": "KNOB_Attack_Dot"
   },
   "part_notes": "body 为父（旋转 pivot 在轴心），cap / indicator 为子对象（原点在帽中心）",
   "filmstrip_deprecated": true,
   "sprite_mode": "body + indicator 分离（见 knob_body_* / knob_pointer / knob_dot）"
  },
  {
   "id": "knob_release",
   "obj": "KNOB_Release_Body",
   "kind": "rotary",
   "box": [
    445,
    95,
    60,
    60
   ],
   "pivot_px": [
    475,
    125
   ],
   "pivot_space": "ref_canvas_950x175",
   "center_mm": [
    0.0,
    -19.05
   ],
   "diameter_mm": 30.48,
   "body_height_mm": 2.8,
   "body_note": "黑滚花身 + 抛光铝帽（见 cap/grip；原厂 sprite 基准）",
   "cap": {
    "diameter_mm": 13.2,
    "height_mm": 1.15,
    "material": "polished_aluminum(metalness 0.5, roughness 0.2, 车削纹)",
    "render_rgb_target": [
     200,
     215
    ],
    "note": "抛光铝顶帽（sprite 实测；非黑坑）"
   },
   "grip": {
    "diameter_mm": 19.3,
    "height_mm": 2.8,
    "knurl_teeth": 29,
    "knurl_depth_mm": 0.42,
    "render_rgb_target": [
     60,
     100
    ]
   },
   "indicator": {
    "type": "dark_dot",
    "diameter_mm": 2.5,
    "center_radius_mm": 5.1,
    "color": "#0A0A0C",
    "note": "帽面小黑点（sprite 实测，非白线）"
   },
   "shadow": {
    "asset_big": "knob_shadow_big.png",
    "asset_small": "knob_shadow_small.png",
    "diameter_mm": 20.0,
    "alpha_center": 150,
    "alpha_edge": 0,
    "offset_mm": [
     -1.2,
     1.4
    ],
    "note": "半透明投影盘（原厂 sprite 自带；不是实体）"
   },
   "angle_min_deg": 210.8,
   "angle_max_deg": 509.7,
   "sweep_deg": 298.9,
   "direction": "cw_increase",
   "angle_convention": "theta_cw_deg_from_12oclock_clockwise",
   "param": "release",
   "value_range": [
    0,
    1
   ],
   "default_value": 0.35,
   "frames": 61,
   "frame_w_px": 120,
   "frame_h_px": 120,
   "frame_w_css": 60,
   "frame_h_css": 60,
   "strip": "knob_attack_61f.png",
   "pivot_in_frame_px": [
    60,
    60
   ],
   "pivot_in_frame_css": [
    30,
    30
   ],
   "alpha_bbox_frame_px": [
    16,
    15,
    98,
    98
   ],
   "parts": {
    "body": "KNOB_Release_Body",
    "cap": "KNOB_Release_Cap",
    "indicator": "KNOB_Release_Dot"
   },
   "part_notes": "body 为父（旋转 pivot 在轴心），cap / indicator 为子对象（原点在帽中心）",
   "filmstrip_deprecated": true,
   "sprite_mode": "body + indicator 分离（见 knob_body_* / knob_pointer / knob_dot）"
  },
  {
   "id": "btn_ratio_20",
   "obj": "BTN_Ratio_20",
   "kind": "push",
   "box": [
    589,
    25,
    17,
    30
   ],
   "visual_box": [
    589.5,
    27.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    597.5,
    41.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "btn_ratio_up.png",
   "asset_down": "btn_ratio_down.png",
   "frame_w_px": 64,
   "frame_h_px": 80,
   "frame_w_css": 32,
   "frame_h_css": 40,
   "button_center_in_frame_px": [
    32,
    40
   ],
   "button_center_in_frame_css": [
    16,
    20
   ],
   "button_face_mm": [
    8.128,
    14.224
   ],
   "seam_mm": 0.508,
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "param": "ratio",
   "ratio_value": 3,
   "label": "20:1"
  },
  {
   "id": "btn_ratio_12",
   "obj": "BTN_Ratio_12",
   "kind": "push",
   "box": [
    589,
    55,
    17,
    30
   ],
   "visual_box": [
    589.5,
    56.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    597.5,
    70.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "btn_ratio_up.png",
   "asset_down": "btn_ratio_down.png",
   "frame_w_px": 64,
   "frame_h_px": 80,
   "frame_w_css": 32,
   "frame_h_css": 40,
   "button_center_in_frame_px": [
    32,
    40
   ],
   "button_center_in_frame_css": [
    16,
    20
   ],
   "button_face_mm": [
    8.128,
    14.224
   ],
   "seam_mm": 0.508,
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "param": "ratio",
   "ratio_value": 2,
   "label": "12:1"
  },
  {
   "id": "btn_ratio_8",
   "obj": "BTN_Ratio_8",
   "kind": "push",
   "box": [
    589,
    85,
    17,
    30
   ],
   "visual_box": [
    589.5,
    85.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    597.5,
    99.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "btn_ratio_up.png",
   "asset_down": "btn_ratio_down.png",
   "frame_w_px": 64,
   "frame_h_px": 80,
   "frame_w_css": 32,
   "frame_h_css": 40,
   "button_center_in_frame_px": [
    32,
    40
   ],
   "button_center_in_frame_css": [
    16,
    20
   ],
   "button_face_mm": [
    8.128,
    14.224
   ],
   "seam_mm": 0.508,
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "param": "ratio",
   "ratio_value": 1,
   "label": "8:1"
  },
  {
   "id": "btn_ratio_4",
   "obj": "BTN_Ratio_4",
   "kind": "push",
   "box": [
    589,
    115,
    17,
    30
   ],
   "visual_box": [
    589.5,
    114.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    597.5,
    128.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "btn_ratio_up.png",
   "asset_down": "btn_ratio_down.png",
   "frame_w_px": 64,
   "frame_h_px": 80,
   "frame_w_css": 32,
   "frame_h_css": 40,
   "button_center_in_frame_px": [
    32,
    40
   ],
   "button_center_in_frame_css": [
    16,
    20
   ],
   "button_face_mm": [
    8.128,
    14.224
   ],
   "seam_mm": 0.508,
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "param": "ratio",
   "ratio_value": 0,
   "label": "4:1"
  },
  {
   "id": "slot_ratio",
   "obj": "PANEL_Faceplate",
   "kind": "recess",
   "box": [
    589.5,
    25.5,
    16,
    118
   ],
   "box_space": "ref_canvas_950x175",
   "depth_mm": 1.0,
   "note": "面板上挖的浅槽；与按键同宽，只在按键缝里露出"
  },
  {
   "id": "slot_meter",
   "obj": "PANEL_Faceplate",
   "kind": "recess",
   "box": [
    826.5,
    25.5,
    16,
    118
   ],
   "box_space": "ref_canvas_950x175",
   "depth_mm": 1.0,
   "note": "面板上挖的浅槽；与按键同宽，只在按键缝里露出"
  },
  {
   "id": "rack_hole_tl",
   "obj": "PANEL_Faceplate",
   "kind": "cutout",
   "box": [
    7,
    7,
    21,
    13
   ],
   "box_space": "ref_canvas_950x175",
   "radius_px": 6.5,
   "shape": "stadium(胶囊)：两端半圆 r=6.5px",
   "size_mm": [
    10.668,
    6.604
   ],
   "chamfer_mm": 0.2,
   "transparent_in_asset": true,
   "note": "真机 19\" 机架安装孔（贯穿）；panel_front_1900x350.png 中该处 alpha=0"
  },
  {
   "id": "rack_hole_tr",
   "obj": "PANEL_Faceplate",
   "kind": "cutout",
   "box": [
    922,
    7,
    21,
    13
   ],
   "box_space": "ref_canvas_950x175",
   "radius_px": 6.5,
   "shape": "stadium(胶囊)：两端半圆 r=6.5px",
   "size_mm": [
    10.668,
    6.604
   ],
   "chamfer_mm": 0.2,
   "transparent_in_asset": true,
   "note": "真机 19\" 机架安装孔（贯穿）；panel_front_1900x350.png 中该处 alpha=0"
  },
  {
   "id": "rack_hole_bl",
   "obj": "PANEL_Faceplate",
   "kind": "cutout",
   "box": [
    7,
    155,
    21,
    13
   ],
   "box_space": "ref_canvas_950x175",
   "radius_px": 6.5,
   "shape": "stadium(胶囊)：两端半圆 r=6.5px",
   "size_mm": [
    10.668,
    6.604
   ],
   "chamfer_mm": 0.2,
   "transparent_in_asset": true,
   "note": "真机 19\" 机架安装孔（贯穿）；panel_front_1900x350.png 中该处 alpha=0"
  },
  {
   "id": "rack_hole_br",
   "obj": "PANEL_Faceplate",
   "kind": "cutout",
   "box": [
    922,
    155,
    21,
    13
   ],
   "box_space": "ref_canvas_950x175",
   "radius_px": 6.5,
   "shape": "stadium(胶囊)：两端半圆 r=6.5px",
   "size_mm": [
    10.668,
    6.604
   ],
   "chamfer_mm": 0.2,
   "transparent_in_asset": true,
   "note": "真机 19\" 机架安装孔（贯穿）；panel_front_1900x350.png 中该处 alpha=0"
  },
  {
   "id": "meter_column",
   "obj": "METER_Column",
   "kind": "static",
   "box": [
    826.5,
    25.5,
    16,
    118
   ],
   "pivot_px": [
    834.5,
    84.5
   ],
   "pivot_space": "ref_canvas_950x175",
   "note": "4 段按钮共用的底板（连续黑条）；原厂 METER 不是旋钮"
  },
  {
   "id": "meter_gr",
   "obj": "METER_Btn_GR",
   "kind": "push_segment",
   "box": [
    826,
    25,
    17,
    30
   ],
   "visual_box": [
    826.5,
    27.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    834.5,
    41.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "meter_col_up.png",
   "asset_active": "meter_col_active_0.png",
   "frame_w_px": 100,
   "frame_h_px": 300,
   "frame_w_css": 50,
   "frame_h_css": 150,
   "canvas_ref_box": [
    809.5,
    9.5,
    50.0,
    150.0
   ],
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "active_state": "pressed_in(按入+变暗，原厂 sprite 证据)",
   "param": "meter",
   "meter_value": 0,
   "label": "GR"
  },
  {
   "id": "meter_p8",
   "obj": "METER_Btn_P8",
   "kind": "push_segment",
   "box": [
    826,
    55,
    17,
    30
   ],
   "visual_box": [
    826.5,
    56.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    834.5,
    70.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "meter_col_up.png",
   "asset_active": "meter_col_active_1.png",
   "frame_w_px": 100,
   "frame_h_px": 300,
   "frame_w_css": 50,
   "frame_h_css": 150,
   "canvas_ref_box": [
    809.5,
    9.5,
    50.0,
    150.0
   ],
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "active_state": "pressed_in(按入+变暗，原厂 sprite 证据)",
   "param": "meter",
   "meter_value": 1,
   "label": "+8"
  },
  {
   "id": "meter_p4",
   "obj": "METER_Btn_P4",
   "kind": "push_segment",
   "box": [
    826,
    85,
    17,
    30
   ],
   "visual_box": [
    826.5,
    85.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    834.5,
    99.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "meter_col_up.png",
   "asset_active": "meter_col_active_2.png",
   "frame_w_px": 100,
   "frame_h_px": 300,
   "frame_w_css": 50,
   "frame_h_css": 150,
   "canvas_ref_box": [
    809.5,
    9.5,
    50.0,
    150.0
   ],
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "active_state": "pressed_in(按入+变暗，原厂 sprite 证据)",
   "param": "meter",
   "meter_value": 2,
   "label": "+4"
  },
  {
   "id": "meter_off",
   "obj": "METER_Btn_OFF",
   "kind": "push_segment",
   "box": [
    826,
    115,
    17,
    30
   ],
   "visual_box": [
    826.5,
    114.5,
    16.0,
    27.0
   ],
   "pivot_px": [
    834.5,
    128.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "asset_up": "meter_col_up.png",
   "asset_active": "meter_col_active_3.png",
   "frame_w_px": 100,
   "frame_h_px": 300,
   "frame_w_css": 50,
   "frame_h_css": 150,
   "canvas_ref_box": [
    809.5,
    9.5,
    50.0,
    150.0
   ],
   "press_axis": [
    0,
    1,
    0
   ],
   "press_mm": 1.1,
   "active_state": "pressed_in(按入+变暗，原厂 sprite 证据)",
   "param": "meter",
   "meter_value": 3,
   "label": "OFF"
  },
  {
   "id": "patch_input",
   "obj": "PATCH_Input",
   "kind": "patch",
   "pivot_px": [
    136.0,
    87.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "radius_mm": 27.9,
   "radius_px": 54.92,
   "rim_mm": 0.6,
   "offset_mm": 0.03,
   "note": "与面板共面的哑光深色圆斑（无台阶、无凹槽）；p90-p10≈2（不吃拉丝高光）"
  },
  {
   "id": "patch_output",
   "obj": "PATCH_Output",
   "kind": "patch",
   "pivot_px": [
    323.0,
    87.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "radius_mm": 27.9,
   "radius_px": 54.92,
   "rim_mm": 0.6,
   "offset_mm": 0.03,
   "note": "与面板共面的哑光深色圆斑（无台阶、无凹槽）；p90-p10≈2（不吃拉丝高光）"
  },
  {
   "id": "patch_attack",
   "obj": "PATCH_Attack",
   "kind": "patch",
   "pivot_px": [
    475.0,
    49.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "radius_mm": 17.3,
   "radius_px": 34.06,
   "rim_mm": 0.6,
   "offset_mm": 0.03,
   "note": "与面板共面的哑光深色圆斑（无台阶、无凹槽）；p90-p10≈2（不吃拉丝高光）"
  },
  {
   "id": "patch_release",
   "obj": "PATCH_Release",
   "kind": "patch",
   "pivot_px": [
    475.0,
    125.0
   ],
   "pivot_space": "ref_canvas_950x175",
   "radius_mm": 17.3,
   "radius_px": 34.06,
   "rim_mm": 0.6,
   "offset_mm": 0.03,
   "note": "与面板共面的哑光深色圆斑（无台阶、无凹槽）；p90-p10≈2（不吃拉丝高光）"
  },
  {
   "id": "meter_needle_single",
   "obj": "METER_Needle",
   "kind": "rotary_single",
   "box": [
    651,
    43,
    130,
    100
   ],
   "box_note": "画布比表窗（130x66）大：pivot 在画布内，前端裁到表窗即可；left/top = 651/43",
   "pivot_px": [
    715.63,
    120.51
   ],
   "pivot_space": "ref_canvas_950x175",
   "pivot_in_image_px": [
    129.26,
    155.02
   ],
   "pivot_in_image_space": "asset_px_at_2x（needle_single.png 260x200 内）",
   "pivot_in_image_css": [
    64.63,
    77.51
   ],
   "image": "needle_single.png",
   "frame_w_px": 260,
   "frame_h_px": 200,
   "frame_w_css": 130,
   "frame_h_css": 100,
   "angle_at_0deg": "指针竖直向上",
   "angle_min_deg": -49.13,
   "angle_max_deg": 51.16,
   "sweep_deg": 100.29,
   "direction": "cw_increase",
   "css_usage": "transform-origin: 129.26px 155.02px（@2x 图像坐标）；transform: rotate(θ_cw deg)",
   "fallback_strip": "needle_21f.png",
   "param": "vu_meter"
  },
  {
   "id": "meter_needle",
   "obj": "METER_Needle",
   "kind": "rotary",
   "box": [
    651,
    43,
    130,
    66
   ],
   "pivot_px": [
    715.63,
    120.51
   ],
   "pivot_mm": [
    122.24,
    -16.769
   ],
   "pivot_note": "pivot 在黄面下方 18.5px（被表框遮住的虚拟轴）",
   "needle_len_mm": 37.85,
   "angle_min_deg": -49.13,
   "angle_max_deg": 51.16,
   "sweep_deg": 100.29,
   "direction": "cw_increase",
   "frames": 21,
   "frame_w_px": 260,
   "frame_h_px": 132,
   "frame_w_css": 130,
   "frame_h_css": 66,
   "strip": "needle_21f.png",
   "pivot_in_frame_px": [
    129.26,
    155.02
   ],
   "pivot_in_frame_css": [
    64.63,
    77.51
   ],
   "alpha_bbox_frame_px": [
    126,
    5,
    134,
    131
   ],
   "param": "vu_meter",
   "geom": {
    "thickness_mm": [
     0.28,
     0.1
    ],
    "segments": 10,
    "verts": 44,
    "note": "锥形片：根部 0.28mm → 尖部 0.10mm"
   }
  },
  {
   "id": "meter_face",
   "obj": "METER_Face",
   "kind": "static",
   "box": [
    645,
    42,
    137,
    60
   ],
   "pivot_px": [
    715.63,
    120.51
   ],
   "texture": "meter_face_tex.png",
   "texture_size": [
    1024,
    448
   ],
   "texture_uv_note": "贴图覆盖整个黄面；刻度弧/红区已烘进贴图（无文字、无 logo）",
   "face_mm": [
    69.596,
    30.48
   ]
  },
  {
   "id": "meter_glass",
   "kind": "layer",
   "obj": "METER_GlassRefl",
   "asset": "meter_glass.png",
   "frame_w_px": 1900,
   "frame_h_px": 350,
   "frame_w_css": 950,
   "frame_h_css": 175,
   "z": 30,
   "alpha_max": 0.247,
   "alpha_peak_on_face": 0.247,
   "note": "只含玻璃反光/边缘压暗，能看穿；盖在指针之上，黄面主体可透出"
  },
  {
   "id": "screw_tl",
   "obj": "SCREW_TL",
   "kind": "static",
   "box": [
    57,
    28,
    14,
    14
   ],
   "pivot_px": [
    64.0,
    35.0
   ],
   "diameter_mm": 7.112
  },
  {
   "id": "screw_tr",
   "obj": "SCREW_TR",
   "kind": "static",
   "box": [
    876,
    29,
    14,
    14
   ],
   "pivot_px": [
    883.0,
    36.0
   ],
   "diameter_mm": 7.112
  },
  {
   "id": "screw_bl",
   "obj": "SCREW_BL",
   "kind": "static",
   "box": [
    57,
    129,
    14,
    14
   ],
   "pivot_px": [
    64.0,
    136.0
   ],
   "diameter_mm": 7.112
  },
  {
   "id": "screw_br",
   "obj": "SCREW_BR",
   "kind": "static",
   "box": [
    876,
    129,
    14,
    14
   ],
   "pivot_px": [
    883.0,
    136.0
   ],
   "diameter_mm": 7.112
  },
  {
   "id": "screw_meter",
   "obj": "SCREW_METER",
   "kind": "static",
   "box": [
    709,
    105,
    14,
    14
   ],
   "pivot_px": [
    716.0,
    112.0
   ],
   "diameter_mm": 7.112
  },
  {
   "id": "knob_body_big",
   "obj": "KNOB_Input_Body",
   "kind": "sprite",
   "asset": "knob_body_big.png",
   "frame_w_px": 192,
   "frame_h_px": 192,
   "frame_w_css": 96.0,
   "frame_h_css": 96.0,
   "pivot_in_image_px": [
    96,
    96
   ],
   "pivot_in_image_css": [
    48.0,
    48.0
   ],
   "pivot_space": "asset_px_at_2x",
   "note": "只有黑滚花 + 铝帽（旋转对称，无任何指示标记）；前端用 CSS rotate 包住整个 body"
  },
  {
   "id": "knob_body_small",
   "obj": "KNOB_Attack_Body",
   "kind": "sprite",
   "asset": "knob_body_small.png",
   "frame_w_px": 120,
   "frame_h_px": 120,
   "frame_w_css": 60.0,
   "frame_h_css": 60.0,
   "pivot_in_image_px": [
    60,
    60
   ],
   "pivot_in_image_css": [
    30.0,
    30.0
   ],
   "pivot_space": "asset_px_at_2x",
   "note": "只有黑滚花 + 铝帽（旋转对称，无任何指示标记）；前端用 CSS rotate 包住整个 body"
  },
  {
   "id": "knob_pointer",
   "obj": "KNOB_Input_Pointer",
   "kind": "indicator",
   "asset": "knob_pointer.png",
   "frame_w_px": 192,
   "frame_h_px": 192,
   "frame_w_css": 96.0,
   "frame_h_css": 96.0,
   "pivot_in_image_px": [
    96,
    96
   ],
   "pivot_in_image_css": [
    48.0,
    48.0
   ],
   "pivot_space": "asset_px_at_2x",
   "indicator_type": "white_line",
   "note": "白线指针，竖直 0°；pivot 在图像中心，配合 body 同 origin 旋转"
  },
  {
   "id": "knob_dot",
   "obj": "KNOB_Attack_Dot",
   "kind": "indicator",
   "asset": "knob_dot.png",
   "frame_w_px": 80,
   "frame_h_px": 80,
   "frame_w_css": 40.0,
   "frame_h_css": 40.0,
   "pivot_in_image_px": [
    40,
    40
   ],
   "pivot_in_image_css": [
    20.0,
    20.0
   ],
   "pivot_space": "asset_px_at_2x",
   "indicator_type": "dark_dot",
   "dot_radius_mm": 5.1,
   "note": "帽面黑点，0° 位置；pivot = 旋钮轴心"
  },
  {
   "id": "needle_blade",
   "obj": "METER_Needle",
   "kind": "sprite",
   "asset": "needle_blade.png",
   "frame_w_px": 360,
   "frame_h_px": 360,
   "frame_w_css": 180.0,
   "frame_h_css": 180.0,
   "pivot_in_image_px": [
    180,
    180
   ],
   "pivot_in_image_css": [
    90.0,
    90.0
   ],
   "pivot_space": "asset_px_at_2x",
   "angle_zero": "vertical_up(0°)",
   "note": "只有针体（无投影）；前端 CSS rotate + 可选 drop-shadow"
  },
  {
   "id": "needle_shadow",
   "obj": "METER_Needle",
   "kind": "sprite",
   "asset": "needle_shadow.png",
   "frame_w_px": 360,
   "frame_h_px": 360,
   "frame_w_css": 180.0,
   "frame_h_css": 180.0,
   "pivot_in_image_px": [
    180,
    180
   ],
   "pivot_in_image_css": [
    90.0,
    90.0
   ],
   "pivot_space": "asset_px_at_2x",
   "offset_in_image_px": [
    2,
    3
   ],
   "alpha": 0.45,
   "blur_px": 1.2,
   "note": "针的投影层（同一 0° 姿态，已偏移）；前端可自行调透明度/偏移，或改用 CSS drop-shadow"
  },
  {
   "id": "panel_base",
   "kind": "layer",
   "obj": "PANEL_Faceplate",
   "asset": "panel_base.png",
   "frame_w_px": 1900,
   "frame_h_px": 350,
   "frame_w_css": 950,
   "frame_h_css": 175,
   "z": 0,
   "contains": [
    "PANEL_Faceplate（拉丝/四角胶囊孔/螺丝）",
    "PATCH_* 共面暗场",
    "slot_ratio / slot_meter 凹槽",
    "METER_Bezel",
    "METER_Face（纸面，无刻度）"
   ],
   "excludes": [
    "KNOB_*",
    "BTN_Ratio_*",
    "METER_Btn_* / METER_Column",
    "METER_Needle",
    "METER_GlassRefl"
   ],
   "note": "四角孔为不透明纯黑（rgb 0 / alpha 255），可直接叠任何底色"
  }
 ],
 "markings": {
  "angle_convention": "theta_cw_deg_from_12oclock_clockwise",
  "knob_scale_ring_radius_px": 49,
  "input_scale": [
   {
    "label": "0",
    "angle_cw": 159.9
   },
   {
    "label": "6",
    "angle_cw": 119.5
   },
   {
    "label": "12",
    "angle_cw": 80.0
   },
   {
    "label": "18",
    "angle_cw": 40.5
   },
   {
    "label": "24",
    "angle_cw": 0.5
   },
   {
    "label": "30",
    "angle_cw": 318.9
   },
   {
    "label": "36",
    "angle_cw": 279.4
   },
   {
    "label": "48",
    "angle_cw": 239.0
   },
   {
    "label": "∞",
    "angle_cw": 198.5
   }
  ],
  "output_scale": "同 input_scale",
  "attack_scale": [
   {
    "label": "1",
    "angle_cw": 221.7
   },
   {
    "label": "3",
    "angle_cw": 311.8
   },
   {
    "label": "5",
    "angle_cw": 45.9
   },
   {
    "label": "7",
    "angle_cw": 140.3
   }
  ],
  "attack_scale_step_deg": 46.4,
  "release_scale": "同 attack_scale",
  "ratio_labels": [
   {
    "label": "20",
    "y_px": 40
   },
   {
    "label": "12",
    "y_px": 71
   },
   {
    "label": "8",
    "y_px": 100
   },
   {
    "label": "4",
    "y_px": 129
   }
  ],
  "ratio_label_x_px": 572,
  "meter_labels": [
   {
    "label": "GR",
    "y_px": 43
   },
   {
    "label": "+8",
    "y_px": 75
   },
   {
    "label": "+4",
    "y_px": 104
   },
   {
    "label": "OFF",
    "y_px": 131
   }
  ],
  "meter_label_x_px": 852,
  "captions": {
   "INPUT": [
    136,
    148
   ],
   "OUTPUT": [
    323,
    148
   ],
   "ATTACK": [
    475,
    11
   ],
   "RELEASE": [
    475,
    162
   ],
   "RATIO": [
    596,
    15
   ],
   "METER": [
    834,
    15
   ]
  },
  "vu_scale": {
   "center_px": [
    715.63,
    120.51
   ],
   "arc_radius_px": 55.0,
   "tick_outer_px": 56.2,
   "tick_inner_px": 44.9,
   "minor_tick_outer_px": 56.2,
   "minor_tick_inner_px": 50.6,
   "angle_convention": "image_angle_deg：0=+x（右），屏幕顺时针为正；theta_cw_deg = image_angle_deg + 90",
   "labels": [
    {
     "label": "-20",
     "angle_image_deg": -132.6,
     "theta_cw_deg": -42.6
    },
    {
     "label": "-10",
     "angle_image_deg": -122.8,
     "theta_cw_deg": -32.8
    },
    {
     "label": "-7",
     "angle_image_deg": -112.5,
     "theta_cw_deg": -22.5
    },
    {
     "label": "-5",
     "angle_image_deg": -104.2,
     "theta_cw_deg": -14.2
    },
    {
     "label": "-3",
     "angle_image_deg": -94.9,
     "theta_cw_deg": -4.9
    },
    {
     "label": "-1",
     "angle_image_deg": -79.9,
     "theta_cw_deg": 10.1
    },
    {
     "label": "0",
     "angle_image_deg": -71.0,
     "theta_cw_deg": 19.0
    },
    {
     "label": "+1",
     "angle_image_deg": -61.6,
     "theta_cw_deg": 28.4
    },
    {
     "label": "+2",
     "angle_image_deg": -52.9,
     "theta_cw_deg": 37.1
    },
    {
     "label": "+3",
     "angle_image_deg": -46.0,
     "theta_cw_deg": 44.0
    }
   ],
   "labels_note": "原厂刻度没有 -2 这一档（实测标签为 -20/-10/-7/-5/-3/-1/0/+1/+2/+3）；角度是标签中心，±1.5°",
   "minor_ticks": {
    "note": "原厂次刻度非等分；建议在相邻主刻度之间线性插入 1 根（视觉近似）",
    "count_per_interval": 1
   },
   "needle_range_image_deg": [
    -139.13,
    -38.84
   ],
   "overtravel_note": "指针左右两端各超出首末刻度约 6.5° / 7.2°（原厂一致），不是越界",
   "red_band": {
    "r_inner_px": 52.8,
    "r_outer_px": 72.7,
    "angle_from_image_deg": -69.6,
    "angle_to_image_deg": -45.1,
    "theta_cw_from_deg": 20.4,
    "theta_cw_to_deg": 44.9,
    "color_srgb": [
     214,
     78,
     48
    ],
    "color_note": "实测原厂红为 (233,65,26)；印刷感建议用降饱和版 (214,78,48)",
    "note": "红区从 0 VU 起、到 +3 止，与刻度弧同心（ring sector）"
   },
   "percent_scale": {
    "arc_radius_px": 41.0,
    "labels": [
     {
      "label": "0",
      "angle_image_deg": -150.2
     },
     {
      "label": "20",
      "angle_image_deg": -139.3
     },
     {
      "label": "40",
      "angle_image_deg": -121.5
     },
     {
      "label": "60",
      "angle_image_deg": -95.8
     },
     {
      "label": "80",
      "angle_image_deg": -67.8
     },
     {
      "label": "100",
      "angle_image_deg": -42.9
     }
    ],
    "confidence": "low",
    "note": "原厂百分比刻度与 dB 刻度混排在同一弧上，低分辨率下数字粘连；角度为粗读（±5°），建议前端按 100%=0VU 理论映射或暂缓绘制"
   },
   "label_radius_px": 72.0,
   "percent_label_radius_px": 32.0,
   "ink_rgb": [
    46,
    44,
    37
   ],
   "ink_rgb_measured_median": [
    39,
    39,
    38
   ],
   "vu_marks": {
    "left": {
     "angle_image_deg": -178.0,
     "radius_px": 65.4
    },
    "right": {
     "angle_image_deg": -2.0,
     "radius_px": 65.4
    }
   },
   "measurement": {
    "source": "refs/uad-1176-gui/UA_1176_meter_OVERLAY-OFF.png",
    "pivot_in_overlay_px": [
     73.63,
     81.51
    ],
    "r_percentiles_px": {
     "5": 11.1,
     "25": 28.6,
     "50": 53.5,
     "75": 78.2,
     "95": 99.4
    },
    "note": "刻度弧外径 = 逐角度墨迹最大半径的中位数 56.18；主刻度内端 p2 = 44.9；次刻度内端中位 50.6；dB 数字带 r 62.5–81.6（中心≈72）；两端 VU 字 r≈65"
   }
  },
  "vu_layer_order": [
   "meter_face_tex(纸面，无刻度)",
   "vu_scale(SVG：刻度+数字+红区)",
   "METER_Needle(needle_single.png 单帧 CSS 旋转，或 needle_21f 逐帧)",
   "METER_Glass(半透明，不遮挡读数)"
  ]
 },
 "params": {
  "order": [
   "input",
   "output",
   "attack",
   "release",
   "ratio",
   "meter",
   "power",
   "bypass"
  ],
  "specs": {
   "input": {
    "type": "float",
    "range": [
     0,
     1
    ],
    "default": 0.42,
    "label": "INPUT"
   },
   "output": {
    "type": "float",
    "range": [
     0,
     1
    ],
    "default": 0.33,
    "label": "OUTPUT"
   },
   "attack": {
    "type": "float",
    "range": [
     0,
     1
    ],
    "default": 0.45,
    "label": "ATTACK"
   },
   "release": {
    "type": "float",
    "range": [
     0,
     1
    ],
    "default": 0.35,
    "label": "RELEASE"
   },
   "ratio": {
    "type": "enum",
    "values": [
     4,
     8,
     12,
     20,
     "ALL"
    ],
    "default": 0,
    "label": "RATIO",
    "note": "0..3 = 4:1/8:1/12:1/20:1；4 = ALL（四键全按下）"
   },
   "meter": {
    "type": "enum",
    "values": [
     "OFF",
     "+4",
     "+8",
     "GR"
    ],
    "default": "GR",
    "label": "METER"
   },
   "power": {
    "type": "bool",
    "default": true,
    "label": "POWER",
    "note": "原厂面板无此控件；由前端 HTML 画（§6.6）"
   },
   "bypass": {
    "type": "bool",
    "default": false,
    "label": "BYPASS",
    "note": "原厂面板无此控件；由宿主/前端提供"
   }
  }
 },
 "assets": {
  "panel_front_1900x350.png": {
   "file": "panel_front_1900x350.png",
   "w": 1900,
   "h": 350,
   "bytes": 499191,
   "md5": "f92b2e8871544e206ccc583254f6057a"
  },
  "panel_front_1900x350_on_dark.png": {
   "file": "panel_front_1900x350_on_dark.png",
   "w": 1900,
   "h": 350,
   "bytes": 579915,
   "md5": "a6b2b83df22bcdb578339c2348ff7a1c"
  },
  "hero_1600x900.png": {
   "file": "hero_1600x900.png",
   "w": 1600,
   "h": 900,
   "bytes": 1323792,
   "md5": "06bdde47e153e3dbec0a8c3056bfeaba"
  },
  "knob_input_61f.png": {
   "file": "knob_input_61f.png",
   "w": 180,
   "h": 10980,
   "bytes": 1585160,
   "md5": "c09c6bfe51ed813c4078b80c63dd5232"
  },
  "knob_attack_61f.png": {
   "file": "knob_attack_61f.png",
   "w": 120,
   "h": 7320,
   "bytes": 588879,
   "md5": "4512b9e9b951f28216b07610e34b4f0e"
  },
  "needle_21f.png": {
   "file": "needle_21f.png",
   "w": 260,
   "h": 2772,
   "bytes": 53599,
   "md5": "354a077583283d35e0c43981923a63b1"
  },
  "needle_single.png": {
   "file": "needle_single.png",
   "w": 260,
   "h": 200,
   "bytes": 29329,
   "md5": "88c017f469b77d3607e68fb05f8a4351"
  },
  "btn_ratio_up.png": {
   "file": "btn_ratio_up.png",
   "w": 64,
   "h": 80,
   "bytes": 2787,
   "md5": "ed3d6282b668c9b6265baad2d42a204f"
  },
  "btn_ratio_down.png": {
   "file": "btn_ratio_down.png",
   "w": 64,
   "h": 80,
   "bytes": 2904,
   "md5": "19d3fb187fb5d4172d1edaa3f78f470b"
  },
  "meter_col_up.png": {
   "file": "meter_col_up.png",
   "w": 100,
   "h": 300,
   "bytes": 23233,
   "md5": "026cce2ee945c2b0b35b97e74e50fa92"
  },
  "meter_col_active_0.png": {
   "file": "meter_col_active_0.png",
   "w": 100,
   "h": 300,
   "bytes": 22984,
   "md5": "36d6e9cf673bfa7fd3ce38601fdce75c"
  },
  "meter_col_active_1.png": {
   "file": "meter_col_active_1.png",
   "w": 100,
   "h": 300,
   "bytes": 23006,
   "md5": "724acf14bbdc28b02e003766236045b6"
  },
  "meter_col_active_2.png": {
   "file": "meter_col_active_2.png",
   "w": 100,
   "h": 300,
   "bytes": 23077,
   "md5": "af79c8380e1f3ee71433179adf4afb92"
  },
  "meter_col_active_3.png": {
   "file": "meter_col_active_3.png",
   "w": 100,
   "h": 300,
   "bytes": 23001,
   "md5": "74817456d97fcd11356077f0463d9229"
  },
  "meter_face_tex.png": {
   "file": "meter_face_tex.png",
   "w": 1024,
   "h": 448,
   "bytes": 300332,
   "md5": "aa18496c642d07d7b3ad92325cc4c708"
  },
  "panel_brushed_tex.png": {
   "file": "panel_brushed_tex.png",
   "w": 2048,
   "h": 512,
   "bytes": 147928,
   "md5": "013320303dfa3d17ead3570de5d9348b"
  },
  "meter_glass_refl_tex.png": {
   "file": "meter_glass_refl_tex.png",
   "w": 512,
   "h": 224,
   "bytes": 6223,
   "md5": "12eb593121db6a2d186dac97c7be5ac7"
  },
  "knob_shadow_big.png": {
   "file": "knob_shadow_big.png",
   "w": 512,
   "h": 512,
   "bytes": 47012,
   "md5": "0737fe93fa8c80893de428d3a8c4e146"
  },
  "knob_shadow_small.png": {
   "file": "knob_shadow_small.png",
   "w": 256,
   "h": 256,
   "bytes": 19527,
   "md5": "b25b49b9547c12f37a29dbd69adb4b43"
  },
  "knob_cap_alu_tex.png": {
   "file": "knob_cap_alu_tex.png",
   "w": 1024,
   "h": 1024,
   "bytes": 277713,
   "md5": "2833ac6508a27548dcd40fba5c61c7a2"
  },
  "knob_body_big.png": {
   "path": "knob_body_big.png",
   "md5": "cd65f59759c71d970a09c328bb38cf93",
   "bytes": 29145
  },
  "knob_body_small.png": {
   "path": "knob_body_small.png",
   "md5": "ac40bdab95a2d8345e4ec83e8a58fd77",
   "bytes": 14612
  },
  "knob_pointer.png": {
   "path": "knob_pointer.png",
   "md5": "4af6d905ffb79e07ff97c9ff740ef3d2",
   "bytes": 17646
  },
  "knob_dot.png": {
   "path": "knob_dot.png",
   "md5": "4d735995a6cf5d11b550ef5c68a08ef4",
   "bytes": 3886
  },
  "needle_blade.png": {
   "path": "needle_blade.png",
   "md5": "2fd31163e49f8b882fc16263246b41fb",
   "bytes": 63327
  },
  "needle_shadow.png": {
   "path": "needle_shadow.png",
   "md5": "2953ac5f577f09126c39a5f40d937494",
   "bytes": 1632
  },
  "panel_base.png": {
   "path": "panel_base.png",
   "md5": "15b554ee0bbb150b8fc021b611673945",
   "bytes": 467195
  },
  "meter_glass.png": {
   "path": "meter_glass.png",
   "md5": "cdd2f3ae67025096c654ee87e24dfadf",
   "bytes": 286957
  }
 },
 "notes": [
  "所有光栅资产为 @2x（scale=2）；frame_w_css / frame_h_css 是逻辑 px，可直接用于 CSS。",
  "filmstrip 竖排：background-position-y = -frameIndex * frame_h_css。",
  "INPUT/OUTPUT：值增大 → 指针逆时针（θ_cw 递减），因为原厂刻度数字沿逆时针递增。",
  "ATTACK/RELEASE：值增大 → 指针顺时针（θ_cw 递增）。",
  "UAD 原厂 filmstrip 的帧序是反的（frame0 = ∞ 位）；我们按语义重排为 frame0 = 最小值。",
  "VU 指针 pivot 在画布下方（虚拟轴）；优先用 needle_single.png + CSS rotate（transform-origin 见组件）。",
  "VU 表盘信息层（刻度+数字+红区）由前端 SVG 绘制；meter_face_tex.png 只有纸面（无刻度/无数字/无红区/无 logo）。",
  "面板底色贴图/模型均无文字与 logo；文字由前端 HTML 绘制（见 markings）。",
  "按钮按压轴 = 面板法线 +Y（面板内）；等价于需求里「按 -Z 朝面板内」。",
  "旋钮 = 低矮平顶哑光黑圆盘（高 3.2mm，无滚花/无穹顶）+ 中心 Ø8.128 平底黑坑（CAP_* 子对象）+ 白色细线指针；面板上另有一圈 1.2mm 宽安装缝。",
  "METER 不是旋钮：是一根 4 段平按钮的竖直长条（段高 28px/段缝 ≈0.15mm），背后有 16x118px 凹槽；四角另有 21x13px 胶囊贯穿孔。",
  "【基准规则】panel.png 只是背景板，其深色圆盘/黑圆心不是旋钮；每个组件的几何与材质一律以各自 sprite 为准（见 docs §18）。",
  "旋钮按原厂 sprite 重建：黑滚花身（大 Ø24.4/38 齿，小 Ø19.3/29 齿）+ 抛光铝帽（Ø19.3/Ø13.2）+ 半透明投影；大旋钮白线指针，小旋钮帽面黑点。"
 ],
 "layer_stack": [
  {
   "z": 0,
   "asset": "panel_base.png",
   "note": "底图（静态件）"
  },
  {
   "z": 10,
   "assets": [
    "knob_body_big.png",
    "knob_body_small.png"
   ],
   "note": "旋钮 body（CSS rotate）"
  },
  {
   "z": 12,
   "assets": [
    "knob_pointer.png",
    "knob_dot.png"
   ],
   "note": "指示件，与 body 同 origin"
  },
  {
   "z": 20,
   "assets": [
    "needle_blade.png",
    "needle_shadow.png"
   ],
   "note": "VU 指针（CSS rotate，pivot 内）"
  },
  {
   "z": 30,
   "asset": "meter_glass.png",
   "note": "玻璃反光（最上）"
  },
  {
   "z": 40,
   "assets": [
    "btn_ratio_up.png",
    "btn_ratio_down.png",
    "meter_col_*.png"
   ],
   "note": "按键按 z 序插在 panel_base 之上"
  }
 ]
};
