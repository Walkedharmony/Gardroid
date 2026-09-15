package com.zeronovel.gardroid.utils;

import android.app.AlertDialog;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;
import com.zeronovel.gardroid.bridge.NativeLib;
import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class AstManager {

    private final Context context;
    private final NativeLib nativeLib;

    public interface AstConfigCallback {
        void onConfigComplete(boolean convertAst, String language);
    }

    public AstManager(Context context) {
        this.context = context;
        this.nativeLib = new NativeLib();
    }

    public boolean containsAstFiles(List<String> filePaths) {
        if (!new SettingsConfig(context).isManagerEnabled(SettingsConfig.KEY_AST_MANAGER)) return false;
        for (String path : filePaths) {
            if (path.toLowerCase().endsWith(".ast")) return true;
        }
        return false;
    }

    public void configureAstExtraction(File archiveFile, List<String> filePaths, AstConfigCallback callback) {
        if (!new SettingsConfig(context).isManagerEnabled(SettingsConfig.KEY_AST_MANAGER)) {
            callback.onConfigComplete(false, null);
            return;
        }

        List<String> candidates = new ArrayList<>();
        for (String path : filePaths) {
            if (path.endsWith(".ast")) {
                candidates.add(path);
                if (candidates.size() >= 5) break;
            }
        }

        if (candidates.isEmpty()) {
            callback.onConfigComplete(false, null);
            return;
        }

        // Langsung scan dan pilih bahasa tanpa konfirmasi awal
        scanAndSelectLanguage(archiveFile, candidates, callback);
    }

    private void scanAndSelectLanguage(File archiveFile, List<String> candidates, AstConfigCallback callback) {
        AlertDialog loadingDialog = new AlertDialog.Builder(context)
                .setMessage("Scanning languages...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        new Thread(() -> {
            Set<String> allLanguages = new HashSet<>();
            long parserPtr = nativeLib.initParser(archiveFile.getAbsolutePath());

            if (parserPtr != 0) {
                File tempDir = context.getCacheDir();

                for (String path : candidates) {
                    File tempFile = new File(tempDir, "scan_" + new File(path).getName());


                    boolean ok = nativeLib.extractFileFromPointer(parserPtr, path, tempFile.getAbsolutePath());

                    if (ok) {
                        String[] langs = nativeLib.detectAstLanguages(tempFile.getAbsolutePath());
                        if (langs != null) {
                            Collections.addAll(allLanguages, langs);
                        }
                        tempFile.delete();

                        if (allLanguages.contains("ja") || allLanguages.contains("en") || allLanguages.contains("jp")) {
                            break;
                        }
                    }
                }
                nativeLib.closeParser(parserPtr);
            }

            new Handler(Looper.getMainLooper()).post(() -> {
                loadingDialog.dismiss();

                if (allLanguages.isEmpty()) {
                    Toast.makeText(context, "Tidak ada bahasa dikenali.", Toast.LENGTH_LONG).show();
                    callback.onConfigComplete(false, null);
                } else {

                    String[] langArray = allLanguages.toArray(new String[0]);
                    new AlertDialog.Builder(context)
                            .setTitle("Pilih Bahasa")
                            .setItems(langArray, (dialog, which) -> {
                                callback.onConfigComplete(true, langArray[which]);
                            })
                            .setCancelable(false)
                            .show();
                }
            });

        }).start();
    }

    public void processAstFile(String extractedFilePath, String targetLang) {
        if (targetLang == null || !extractedFilePath.endsWith(".ast")) return;
        File astFile = new File(extractedFilePath);
        if (!astFile.exists()) return;

        String outPath = extractedFilePath.substring(0, extractedFilePath.lastIndexOf(".")) + "_" + targetLang + ".txt";
        nativeLib.extractAstText(extractedFilePath, outPath, targetLang);
    }
}