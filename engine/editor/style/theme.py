"""
GE Editor Dark Theme
다크 테마 QSS 스타일시트 및 색상 팔레트 정의
"""

# ============================================================
# Color Palette
# ============================================================
COLORS = {
    # Background layers
    "bg_base":        "#1a1a2e",   # 최하위 배경
    "bg_panel":       "#16213e",   # 패널 배경
    "bg_widget":      "#0f3460",   # 위젯 배경
    "bg_hover":       "#1a4a7a",   # 호버 배경
    "bg_selected":    "#e94560",   # 선택 강조색
    "bg_header":      "#0d1b2a",   # 헤더 배경

    # Text
    "text_primary":   "#e0e0e0",   # 기본 텍스트
    "text_secondary": "#8899aa",   # 보조 텍스트
    "text_dim":       "#556677",   # 흐린 텍스트
    "text_accent":    "#00d4ff",   # 강조 텍스트 (씨안)
    "text_warning":   "#ffaa00",   # 경고 텍스트

    # Borders
    "border":         "#1e3a5f",   # 기본 경계선
    "border_focus":   "#00d4ff",   # 포커스 경계선
    "border_light":   "#2a5080",   # 밝은 경계선

    # Accent
    "accent":         "#e94560",   # 메인 강조색 (딥핑크/레드)
    "accent_blue":    "#00d4ff",   # 보조 강조색 (씨안)
    "accent_green":   "#00ff88",   # 성공/활성화 색상
    "accent_orange":  "#ff7700",   # 경고 색상
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
    color: {COLORS['accent_blue']};
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
    color: {COLORS['accent_blue']};
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
    background-color: {COLORS['accent_blue']};
}}

/* --- Panel Headers --- */
#panel_header {{
    background-color: {COLORS['bg_header']};
    color: {COLORS['accent_blue']};
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
    color: {COLORS['accent_blue']};
}}

QTreeWidget::item:selected {{
    background-color: {COLORS['bg_widget']};
    color: {COLORS['accent_blue']};
    border-left: 2px solid {COLORS['accent_blue']};
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
    background-color: {COLORS['accent_blue']};
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
    background-color: {COLORS['accent_blue']};
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
    border: 1px solid {COLORS['accent_blue']};
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
    border: 1px solid {COLORS['accent_blue']};
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
    border-color: {COLORS['accent_blue']};
    color: {COLORS['accent_blue']};
}}

QPushButton:pressed {{
    background-color: {COLORS['accent']};
    color: white;
    border-color: {COLORS['accent']};
}}

QPushButton#btn_create {{
    background-color: {COLORS['bg_widget']};
    border: 1px solid {COLORS['accent_blue']};
    color: {COLORS['accent_blue']};
    font-weight: bold;
}}

QPushButton#btn_create:hover {{
    background-color: {COLORS['accent_blue']};
    color: {COLORS['bg_base']};
}}

QPushButton#btn_delete {{
    border: 1px solid {COLORS['accent']};
    color: {COLORS['accent']};
}}

QPushButton#btn_delete:hover {{
    background-color: {COLORS['accent']};
    color: white;
}}

QPushButton#btn_mode_active {{
    background-color: {COLORS['bg_widget']};
    color: {COLORS['accent_blue']};
    border: 1px solid {COLORS['accent_blue']};
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
    color: {COLORS['accent_blue']};
    font-size: 12px;
    font-weight: bold;
    padding: 4px 0px;
}}

QLabel#label_axis_x {{
    color: #ff6666;
    font-weight: bold;
    font-size: 11px;
    min-width: 14px;
}}

QLabel#label_axis_y {{
    color: #66ff66;
    font-weight: bold;
    font-size: 11px;
    min-width: 14px;
}}

QLabel#label_axis_z {{
    color: #6699ff;
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
    background-color: {COLORS['accent_blue']};
    border-color: {COLORS['accent_blue']};
}}

QCheckBox::indicator:hover {{
    border-color: {COLORS['accent_blue']};
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
    border-color: {COLORS['accent_blue']};
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
    selection-color: {COLORS['accent_blue']};
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
    color: {COLORS['accent_blue']};
}}

QToolButton:pressed {{
    background-color: {COLORS['bg_widget']};
}}

QToolButton:checked {{
    background-color: {COLORS['bg_widget']};
    border-color: {COLORS['accent_blue']};
    color: {COLORS['accent_blue']};
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
