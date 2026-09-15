package com.zeronovel.gardroid.utils;

import android.content.Context;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Properties;

public class SettingsConfig {
    private static final String FILE_NAME = "Settings.ini";
    private final File settingsFile;
    private final Properties properties;

    public static final String KEY_SCN_MANAGER = "ScnManager";
    public static final String KEY_KS_MANAGER = "KsManager";
    public static final String KEY_AST_MANAGER = "AstManager";
    public static final String KEY_TLG_CONVERT_PNG = "TlgConvertPng";

    public SettingsConfig(Context context) {
        settingsFile = new File(context.getExternalFilesDir(null), FILE_NAME);
        properties = new Properties();
        loadSettings();
    }

    private void loadSettings() {
        if (!settingsFile.exists()) {
            // Set default OFF
            properties.setProperty(KEY_SCN_MANAGER, "OFF");
            properties.setProperty(KEY_KS_MANAGER, "OFF");
            properties.setProperty(KEY_AST_MANAGER, "OFF");
            properties.setProperty(KEY_TLG_CONVERT_PNG, "OFF");
            saveSettings();
        } else {
            try (FileInputStream in = new FileInputStream(settingsFile)) {
                properties.load(in);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public void saveSettings() {
        try (FileOutputStream out = new FileOutputStream(settingsFile)) {
            properties.store(out, "Gardroid Settings");
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public boolean isManagerEnabled(String key) {
        String value = properties.getProperty(key, "OFF");
        return "ON".equalsIgnoreCase(value);
    }

    public void setManagerEnabled(String key, boolean enabled) {
        properties.setProperty(key, enabled ? "ON" : "OFF");
        saveSettings();
    }
}
