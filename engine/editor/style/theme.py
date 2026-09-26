"""
GE Editor Dark Theme
다크 테마 QSS 스타일시트 및 색상 팔레트 정의

Blender-inspired minimal dark theme with orange accent:
- Dark, low-saturation gray backgrounds
- Orange as the main accent color
- Light gray text and icons for contrast
"""

import sys

# ============================================================
# Color Palette
# ============================================================
COLORS = {
    # Background layers — Blender-inspired dark grays
    "bg_base":        "#1d1d1d",   # 최하위 배경 (very dark gray)
    "bg_panel":       "#2d2d2d",   # 패널 배경 (dark gray)
    "bg_widget":      "#3d3d3d",   # 위젯 배경 (medium dark gray)
    "bg_hover":       "#424242",   # 호버 배경
    "bg_selected":    "#4a4a4a",   # 선택 강조 배경
    "bg_header":      "#252525",   # 헤더 배경

    # Text — Light gray for contrast
    "text_primary":   "#e0e0e0",   # 기본 텍스트 (light gray)
    "text_secondary": "#b0b0b0",   # 보조 텍스트 (medium light gray)
    "text_dim":       "#707070",   # 흐린 텍스트 (medium gray)
    "text_accent":    "#ff8c42",   # 강조 텍스트 (orange accent)
    "text_warning":   "#ffb347",   # 경고 텍스트 (lighter orange)

    # Borders
    "border":         "#404040",   # 기본 경계선
    "border_focus":   "#ff8c42",   # 포커스 경계선 (orange)
    "border_light":   "#505050",   # 밝은 경계선

    # Accent — Orange as main accent color
    "accent":         "#ff8c42",   # 메인 강조색 (orange)
    "accent_blue":    "#ff8c42",   # accent와 동일 값 (다른 파일에서 참조하는 별칭, 하위 호환용)
    "accent_green":   "#7cb342",   # 성공/활성화 (muted green)
    "accent_orange":  "#ff8c42",   # 경고 (same as main accent)
    "accent_danger":  "#ef5350",   # 삭제/위험 전용 (muted red)
}

# ============================================================
# Main QSS Stylesheet
# ============================================================
DARK_THEME_QSS = f"""
/* =====================================================
   GE Editor Dark Theme
   ===================================================== */

/* --- Global --- */
QWidget {{
    background-color: {COLORS['bg_panel']};
    color: {COLORS['text_primary']};
    font-family: 'Segoe UI', 'Arial', sans-serif;
    font-size: 12px;
    border: none;
    outline: none;
}}

/* --- Main Window --- */
QMainWindow {{
    background-color: {COLORS['bg_base']};
}}

QMainWindow::separator {{
    background-color: {COLORS['border']};
    width: 1px;
    height: 1px;
}}

/* --- Menu Bar --- */
QMenuBar {{
    background-color: {COLORS['bg_header']};
    color: {COLORS['text_primary']};
    border-bottom: 1px solid {COLORS['border']};
    padding: 2px 4px;
    font-size: 12px;
}}

QMenuBar::item {{
    background-color: transparent;
    padding: 4px 10px;
    border-radius: 4px;
}}

QMenuBar::item:selected {{
    background-color: {COLORS['bg_hover']};
    color: {COLORS['accent']};
}}

QMenuBar::item:pressed {{
    background-color: {COLORS['bg_selected']};
}}

QMenu {{
    background-color: {COLORS['bg_header']};
    border: 1px solid {COLORS['border_light']};
    border-radius: 4px;
    padding: 4px 0px;
}}

QMenu::item {{
    padding: 5px 24px 5px 16px;
    color: {COLORS['text_primary']};
}}

QMenu::item:selected {{
    background-color: {COLORS['bg_hover']};
    color: {COLORS['accent']};
}}

QMenu::separator {{
    height: 1px;
    background-color: {COLORS['border']};
    margin: 4px 8px;
}}

/* --- Splitter --- */
QSplitter::handle {{
    background-color: {COLORS['border']};
}}

QSplitter::handle:horizontal {{
    width: 2px;
    background-color: {COLORS['border']};
}}

QSplitter::handle:vertical {{
    height: 2px;
    background-color: {COLORS['border']};
}}

QSplitter::handle:hover {{
    background-color: {COLORS['accent']};
}}

/* --- Panel Headers --- */
#panel_header {{
    background-color: {COLORS['bg_header']};
    color: {COLORS['text_secondary']};
    font-size: 11px;
    font-weight: bold;
    letter-spacing: 1px;
    padding: 5px 10px;
    border-bottom: 1px solid {COLORS['border']};
    text-transform: uppercase;
}}

/* --- Tree Widget (Scene Hierarchy) --- */
QTreeWidget {{
    background-color: {COLORS['bg_panel']};
    alternate-background-color: {COLORS['bg_base']};
    border: none;
    border-radius: 0px;
    color: {COLORS['text_primary']};
    selection-background-color: transparent;
    show-decoration-selected: 1;
}}

QTreeWidget::item {{
    height: 24px;
    padding-left: 4px;
    border-radius: 3px;
    color: {COLORS['text_primary']};
}}

QTreeWidget::item:hover {{
    background-color: {COLORS['bg_hover']};
    color: {COLORS['accent']};
}}

QTreeWidget::item:selected {{
    background-color: {COLORS['bg_selected']};
    color: {COLORS['accent']};
    border-left: 2px solid {COLORS['accent']};
}}

QTreeWidget::branch:has-children:!has-siblings:closed,
QTreeWidget::branch:closed:has-children:has-siblings {{
    border-image: none;
    image: none;
    color: {COLORS['text_secondary']};
}}

QTreeWidget::branch:open:has-children:!has-siblings,
QTreeWidget::branch:open:has-children:has-siblings {{
    border-image: none;
    image: none;
}}

QHeaderView::section {{
    background-color: {COLORS['bg_header']};
    color: {COLORS['text_secondary']};
    border: none;
    border-bottom: 1px solid {COLORS['border']};
    padding: 4px 8px;
    font-size: 11px;
    font-weight: bold;
}}

/* --- Scroll Area & Bar --- */
QScrollArea {{
    background-color: {COLORS['bg_panel']};
    border: none;
}}

QScrollBar:vertical {{
    background-color: {COLORS['bg_panel']};
    width: 8px;
    border-radius: 4px;
    margin: 0;
}}

QScrollBar::handle:vertical {{
    background-color: {COLORS['border_light']};
    border-radius: 4px;
    min-height: 20px;
}}

QScrollBar::handle:vertical:hover {{
    background-color: {COLORS['accent']};
}}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
    height: 0px;
}}

QScrollBar:horizontal {{
    background-color: {COLORS['bg_panel']};
    height: 8px;
    border-radius: 4px;
    margin: 0;
}}

QScrollBar::handle:horizontal {{
    background-color: {COLORS['border_light']};
    border-radius: 4px;
    min-width: 20px;
}}

QScrollBar::handle:horizontal:hover {{
    background-color: {COLORS['accent']};
}}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
    width: 0px;
}}

/* --- Line Edit --- */
QLineEdit {{
    background-color: {COLORS['bg_base']};
    color: {COLORS['text_primary']};
    border: 1px solid {COLORS['border']};
    border-radius: 4px;
    padding: 3px 7px;
    selection-background-color: {COLORS['bg_widget']};
}}

QLineEdit:focus {{
    border: 1px solid {COLORS['accent']};
    background-color: {COLORS['bg_panel']};
}}

QLineEdit:hover {{
    border: 1px solid {COLORS['border_light']};
}}

/* --- Double Spin Box --- */
QDoubleSpinBox {{
    background-color: {COLORS['bg_base']};
    color: {COLORS['text_primary']};
    border: 1px solid {COLORS['border']};
    border-radius: 4px;
    padding: 2px 4px;
}}

QDoubleSpinBox:focus {{
    border: 1px solid {COLORS['accent']};
}}

QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {{
    background-color: {COLORS['bg_widget']};
    border: none;
    width: 14px;
    border-radius: 2px;
}}

QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {{
    background-color: {COLORS['bg_hover']};
}}

/* --- Push Button --- */
QPushButton {{
    background-color: {COLORS['bg_widget']};
    color: {COLORS['text_primary']};
    border: 1px solid {COLORS['border_light']};
    border-radius: 5px;
    padding: 5px 12px;
    font-size: 12px;
}}

QPushButton:hover {{
    background-color: {COLORS['bg_hover']};
    border-color: {COLORS['accent']};
    color: {COLORS['accent']};
}}

QPushButton:pressed {{
    background-color: {COLORS['accent']};
    color: {COLORS['bg_base']};
    border-color: {COLORS['accent']};
}}

QPushButton#btn_create {{
    background-color: {COLORS['bg_widget']};
    border: 1px solid {COLORS['accent']};
    color: {COLORS['accent']};
    font-weight: bold;
}}

QPushButton#btn_create:hover {{
    background-color: {COLORS['accent']};
    color: {COLORS['bg_base']};
}}

QPushButton#btn_delete {{
    border: 1px solid {COLORS['accent_danger']};
    color: {COLORS['accent_danger']};
}}

QPushButton#btn_delete:hover {{
    background-color: {COLORS['accent_danger']};
    color: {COLORS['text_primary']};
}}

QPushButton#btn_mode_active {{
    background-color: {COLORS['bg_widget']};
    color: {COLORS['accent']};
    border: 1px solid {COLORS['accent']};
    font-weight: bold;
}}

QPushButton#btn_mode_inactive {{
    background-color: transparent;
    color: {COLORS['text_secondary']};
    border: 1px solid transparent;
}}

QPushButton#btn_mode_inactive:hover {{
    background-color: {COLORS['bg_hover']};
    color: {COLORS['text_primary']};
}}

/* --- Label --- */
QLabel {{
    color: {COLORS['text_primary']};
    background-color: transparent;
}}

QLabel#label_section {{
    color: {COLORS['text_secondary']};
    font-size: 10px;
    font-weight: bold;
    letter-spacing: 1px;
    padding: 4px 0px 2px 0px;
    text-transform: uppercase;
}}

QLabel#label_component_title {{
    color: {COLORS['accent']};
    font-size: 12px;
    font-weight: bold;
    padding: 4px 0px;
}}

QLabel#label_axis_x {{
    color: #d97a7a;
    font-weight: bold;
    font-size: 11px;
    min-width: 14px;
}}

QLabel#label_axis_y {{
    color: #7ac98a;
    font-weight: bold;
    font-size: 11px;
    min-width: 14px;
}}

QLabel#label_axis_z {{
    color: #7a9fd9;
    font-weight: bold;
    font-size: 11px;
    min-width: 14px;
}}

/* --- Check Box --- */
QCheckBox {{
    color: {COLORS['text_primary']};
    spacing: 6px;
}}

QCheckBox::indicator {{
    width: 14px;
    height: 14px;
    border: 1px solid {COLORS['border_light']};
    border-radius: 3px;
    background-color: {COLORS['bg_base']};
}}

QCheckBox::indicator:checked {{
    background-color: {COLORS['accent']};
    border-color: {COLORS['accent']};
}}

QCheckBox::indicator:hover {{
    border-color: {COLORS['accent']};
}}

/* --- Combo Box --- */
QComboBox {{
    background-color: {COLORS['bg_base']};
    color: {COLORS['text_primary']};
    border: 1px solid {COLORS['border']};
    border-radius: 4px;
    padding: 3px 8px;
}}

QComboBox:hover {{
    border-color: {COLORS['border_light']};
}}

QComboBox:focus {{
    border-color: {COLORS['accent']};
}}

QComboBox::drop-down {{
    border: none;
    width: 20px;
}}

QComboBox QAbstractItemView {{
    background-color: {COLORS['bg_header']};
    color: {COLORS['text_primary']};
    border: 1px solid {COLORS['border_light']};
    selection-background-color: {COLORS['bg_hover']};
    selection-color: {COLORS['accent']};
}}

/* --- Status Bar --- */
QStatusBar {{
    background-color: {COLORS['bg_header']};
    color: {COLORS['text_secondary']};
    border-top: 1px solid {COLORS['border']};
    font-size: 11px;
}}

QStatusBar#status_bar_main {{
    background-color: {COLORS['bg_header']};
    padding: 2px 8px;
}}

/* --- Group Box --- */
QGroupBox {{
    color: {COLORS['text_secondary']};
    border: 1px solid {COLORS['border']};
    border-radius: 6px;
    margin-top: 12px;
    padding-top: 8px;
    font-size: 11px;
    font-weight: bold;
}}

QGroupBox::title {{
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 4px;
    color: {COLORS['text_secondary']};
}}

/* --- Tool Bar --- */
QToolBar {{
    background-color: {COLORS['bg_header']};
    border-bottom: 1px solid {COLORS['border']};
    spacing: 4px;
    padding: 2px 4px;
}}

QToolButton {{
    background-color: transparent;
    color: {COLORS['text_primary']};
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 8px;
    font-size: 12px;
}}

QToolButton:hover {{
    background-color: {COLORS['bg_hover']};
    border-color: {COLORS['border_light']};
    color: {COLORS['accent']};
}}

QToolButton:pressed {{
    background-color: {COLORS['bg_widget']};
}}

QToolButton:checked {{
    background-color: {COLORS['bg_widget']};
    border-color: {COLORS['accent']};
    color: {COLORS['accent']};
}}

/* --- Tooltip --- */
QToolTip {{
    background-color: {COLORS['bg_header']};
    color: {COLORS['text_primary']};
    border: 1px solid {COLORS['border_light']};
    border-radius: 4px;
    padding: 4px 8px;
    font-size: 11px;
}}
"""


def apply_theme(app):
    """QApplication에 다크 테마 적용"""
    app.setStyleSheet(DARK_THEME_QSS)


def apply_dark_title_bar(window):
    """
    Windows 네이티브 타이틀바(최소화/최대화/닫기 버튼이 있는 그 줄)를 다크 모드로 전환한다.

    이 줄은 OS가 직접 그리는 영역이라 Qt 스타일시트(DARK_THEME_QSS)로는 절대 손댈 수 없다 -
    그래서 나머지 UI는 전부 다크인데 타이틀바만 밝은 색으로 남아 튀어 보이는 문제가 있었다.
    Windows 10 1809+/11의 DWM(`DwmSetWindowAttribute` + `DWMWA_USE_IMMERSIVE_DARK_MODE`)을
    직접 호출해서 해결한다 - Qt에 대응하는 크로스플랫폼 API가 없어 ctypes로 직접 부른다.

    실패해도(다른 OS, 구버전 Windows, DWM 비활성 등) 예외를 삼키고 조용히 넘어간다 - 이건
    있으면 좋은 화장(化粧)이지 없다고 에디터가 못 쓰게 되면 안 된다.
    """
    if sys.platform != "win32":
        return
    try:
        import ctypes

        hwnd = int(window.winId())
        value = ctypes.c_int(1)
        # 20 = Windows 10 20H1(2004)+ / 11. 그보다 오래된 1809~1909 빌드는 19번을 썼다 -
        # 20번이 실패하면(음수 HRESULT) 구버전 값으로 한 번 더 시도.
        hr = ctypes.windll.dwmapi.DwmSetWindowAttribute(
            hwnd, 20, ctypes.byref(value), ctypes.sizeof(value)
        )
        if hr != 0:
            ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd, 19, ctypes.byref(value), ctypes.sizeof(value)
            )
    except Exception as e:
        print(f"[Theme] 다크 타이틀바 적용 실패(무시하고 계속 진행): {e}")
