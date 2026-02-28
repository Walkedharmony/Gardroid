package com.zeronovel.gardroid.utils;

import android.app.AlertDialog;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;
import com.zeronovel.gardroid.bridge.NativeLib;
import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class KsPackerManager {

    private final Context context;
    private final NativeLib nativeLib;

    public KsPackerManager(Context context) {
        this.context = context;
        this.nativeLib = new NativeLib();
    }

    private static class KsPair {
        String ksPath;
        String txtPath;
        String baseName;
    }

    public void processSelection(List<String> selectedPaths) {
        Map<String, String> ksFiles = new HashMap<>();
        Map<String, String> txtFiles = new HashMap<>();

        for (String path : selectedPaths) {
            File f = new File(path);
            if (f.isDirectory()) scanDirectory(f, ksFiles, txtFiles);
            else addFileToMap(f, ksFiles, txtFiles);
        }

        List<KsPair> pairsToProcess = new ArrayList<>();
        for (Map.Entry<String, String> entry : ksFiles.entrySet()) {
            String baseName = entry.getKey();
            if (txtFiles.containsKey(baseName)) {
                KsPair pair = new KsPair();
                pair.baseName = baseName;
                pair.ksPath = entry.getValue();
                pair.txtPath = txtFiles.get(baseName);
                pairsToProcess.add(pair);
            }
        }

        if (pairsToProcess.isEmpty()) {
            Toast.makeText(context, "Tidak ada pasangan file .ks dan .txt yang cocok.", Toast.LENGTH_LONG).show();
            return;
        }

        executeRepack(pairsToProcess);
    }

    private void scanDirectory(File dir, Map<String, String> ksMap, Map<String, String> txtMap) {
        File[] files = dir.listFiles();
        if (files == null) return;
        for (File f : files) {
            if (f.isDirectory()) scanDirectory(f, ksMap, txtMap);
            else addFileToMap(f, ksMap, txtMap);
        }
    }

    private void addFileToMap(File f, Map<String, String> ksMap, Map<String, String> txtMap) {
        String name = f.getName().toLowerCase();
        String path = f.getAbsolutePath();
        if (name.endsWith(".ks")) {
            ksMap.put(name.substring(0, name.lastIndexOf(".ks")), path);
        } else if (name.endsWith(".ks.txt")) {
            txtMap.put(name.substring(0, name.lastIndexOf(".ks.txt")), path);
        }
    }

    private void executeRepack(List<KsPair> pairs) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle("Repacking Kirikiri KS...");
        builder.setCancelable(false);

        android.widget.LinearLayout layout = new android.widget.LinearLayout(context);
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 10);

        final android.widget.ProgressBar progressBar = new android.widget.ProgressBar(context, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(pairs.size());
        layout.addView(progressBar);

        final android.widget.TextView tvStatus = new android.widget.TextView(context);
        tvStatus.setText("Menyiapkan file...");
        layout.addView(tvStatus);
        builder.setView(layout);

        AlertDialog progressDialog = builder.create();
        progressDialog.show();

        new Thread(() -> {
            int successCount = 0;
            long lastUpdateTime = 0;

            for (int i = 0; i < pairs.size(); i++) {
                KsPair pair = pairs.get(i);
                int progress = i + 1;

                long currentTime = System.currentTimeMillis();
                if (currentTime - lastUpdateTime > 100 || progress == pairs.size()) {
                    lastUpdateTime = currentTime;
                    new Handler(Looper.getMainLooper()).post(() -> {
                        progressBar.setProgress(progress);
                        tvStatus.setText("Repacking (" + progress + " / " + pairs.size() + ")\n" + pair.baseName + ".ks");
                    });
                }

                File originalKsFile = new File(pair.ksPath);
                File newDir = new File(originalKsFile.getParentFile(), "_new_ks");
                if (!newDir.exists()) newDir.mkdirs();
                String outPath = new File(newDir, originalKsFile.getName()).getAbsolutePath();

                if (nativeLib.repackKsText(pair.ksPath, pair.txtPath, outPath) == 1) successCount++;
            }

            int finalSuccess = successCount;
            new Handler(Looper.getMainLooper()).post(() -> {
                progressDialog.dismiss();
                new AlertDialog.Builder(context)
                        .setTitle("Hasil Repack KS")
                        .setMessage("Selesai!\nBerhasil memproses: " + finalSuccess + " file.")
                        .setPositiveButton("OK", null)
                        .show();
            });
        }).start();
    }
}