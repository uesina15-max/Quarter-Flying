"""
editor/config_manager.py

config.json 로직 통합
"""

import json
import os


class ConfigManager:
    """
    설정 파일 관리자
    """
    
    def __init__(self, config_path="config.json"):
        self.config_path = config_path
        self.config = self._load_default_config()
        self.load_config()
    
    def _load_default_config(self):
        """기본 설정 로드"""
        return {
            "window": {
                "title": "Quarter Flying Editor",
                "width": 1600,
                "height": 900,
                "vsync": True
            },
            "paths": {
                "assetRoot": "./",
                "shaderRoot": "./assets/shaders/"
            },
            "engine": {
                "numWorkerThreads": 0,
                "logFrameInterval": 60
            }
        }
    
    def load_config(self):
        """설정 파일 로드"""
        if os.path.exists(self.config_path):
            try:
                with open(self.config_path, 'r') as f:
                    loaded_config = json.load(f)
                    self._merge_config(loaded_config)
                print(f"[Config] Loaded config from {self.config_path}")
            except Exception as e:
                print(f"[Config] Failed to load config: {e}")
    
    def _merge_config(self, loaded_config):
        """설정 병합"""
        def merge_dict(default, loaded):
            for key, value in loaded.items():
                if key in default and isinstance(default[key], dict) and isinstance(value, dict):
                    merge_dict(default[key], value)
                else:
                    default[key] = value
        
        merge_dict(self.config, loaded_config)
    
    def save_config(self):
        """설정 파일 저장"""
        try:
            with open(self.config_path, 'w') as f:
                json.dump(self.config, f, indent=4)
            print(f"[Config] Saved config to {self.config_path}")
        except Exception as e:
            print(f"[Config] Failed to save config: {e}")
    
    def get(self, *keys, default=None):
        """설정 값 가져오기"""
        value = self.config
        for key in keys:
            if isinstance(value, dict) and key in value:
                value = value[key]
            else:
                return default
        return value
    
    def set(self, *keys, value):
        """설정 값 설정"""
        config = self.config
        for key in keys[:-1]:
            if key not in config:
                config[key] = {}
            config = config[key]
        config[keys[-1]] = value
