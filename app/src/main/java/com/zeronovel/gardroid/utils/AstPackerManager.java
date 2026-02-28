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

public class AstPackerManager {

    private final Context context;
    private final NativeLib nativeLib;

    public AstPackerManager(Context context) {
        this.context = context;
        this.nativeLib = new NativeLib();
    }

    private static class AstPair {
        String astPath;
        String txtPath;
        String baseName;
    }

    public void processSelection(List<String> selectedPaths) {
        Map<String, String> astFiles = new HashMap<>();
        Map<String, String> txtFiles = new HashMap<>();

        for (String path : selectedPaths) {
            File f = new File(path);
            if (f.isDirectory()) {
                scanDirectory(f, astFiles, txtFiles);
            } else {
                addFileToMap(f, astFiles, txtFiles);
            }
        }

        List<AstPair> pairsToProcess = new ArrayList<>();
        for (Map.Entry<String, String> entry : astFiles.entrySet()) {
            String baseName = entry.getKey();
            if (txtFiles.containsKey(baseName)) {
                AstPair pair = new AstPair();
                pair.baseName = baseName;
                pair.astPath = entry.getValue();
                pair.txtPath = txtFiles.get(baseName);
                pairsToProcess.add(pair);
            }
        }

        if (pairsToProcess.isEmpty()) {
            Toast.makeText(context, "Tidak ada pasangan file .ast dan .txt yang cocok ditemukan.", Toast.LENGTH_LONG).show();
            return;
        }

        showLanguageDialog(pairsToProcess);
    }

    private void scanDirectory(File dir, Map<String, String> astMap, Map<String, String> txtMap) {
        File[] files = dir.listFiles();
        if (files == null) return;
        for (File f : files) {
            if (f.isDirectory()) scanDirectory(f, astMap, txtMap); // Recursive
            else addFileToMap(f, astMap, txtMap);
        }
    }

    private void addFileToMap(File f, Map<String, String> astMap, Map<String, String> txtMap) {
        String name = f.getName().toLowerCase();
        String path = f.getAbsolutePath();
        if (name.endsWith(".ast") && !name.contains(".new.ast")) {
            String baseName = name.substring(0, name.lastIndexOf(".ast"));
            astMap.put(baseName, path);
        } else if (name.endsWith(".txt")) {
            String baseName = name.substring(0, name.lastIndexOf(".txt"));
            if (baseName.endsWith("_ja") || baseName.endsWith("_en") || baseName.endsWith("_cn")) {
                baseName = baseName.substring(0, baseName.length() - 3);
            }
            txtMap.put(baseName, path);
        }
    }

    private void showLanguageDialog(List<AstPair> pairs) {
        AlertDialog loadingDialog = new AlertDialog.Builder(context)
                .setMessage("Mendeteksi bahasa dari AST...")
                .setCancelable(false)
                .show();

        new Thread(() -> {
            String sampleAstPath = pairs.get(0).astPath;
            String[] detectedLanguages = nativeLib.detectAstLanguages(sampleAstPath);

            final String[] finalLanguages = (detectedLanguages != null && detectedLanguages.length > 0)
                    ? detectedLanguages
                    : new String[]{"cn", "ja", "en", "jp"};

            new Handler(Looper.getMainLooper()).post(() -> {
                loadingDialog.dismiss();

                new AlertDialog.Builder(context)
                        .setTitle("Repack " + pairs.size() + " File AST")
                        .setItems(finalLanguages, (dialog, which) -> {
                            String targetLang = finalLanguages[which];
                            executeRepack(pairs, targetLang);
                        })
                        .setNegativeButton("Batal", null)
                        .show();
            });
        }).start();
    }

    private void executeRepack(List<AstPair> pairs, String targetLang) {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setTitle("Repacking AST...");
        builder.setCancelable(false);

        android.widget.LinearLayout layout = new android.widget.LinearLayout(context);
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 10);

        final android.widget.ProgressBar progressBar = new android.widget.ProgressBar(context, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(pairs.size());
        layout.addView(progressBar);

        final android.widget.TextView tvStatus = new android.widget.TextView(context);
        tvStatus.setText("Menyiapkan file...");
        tvStatus.setPadding(0, 20, 0, 0);
        layout.addView(tvStatus);

        builder.setView(layout);
        AlertDialog progressDialog = builder.create();
        progressDialog.show();

        new Thread(() -> {
            int successCount = 0;
            int mismatchCount = 0;
            long lastUpdateTime = 0;

            for (int i = 0; i < pairs.size(); i++) {
                AstPair pair = pairs.get(i);
                int progress = i + 1;

                long currentTime = System.currentTimeMillis();
                if (currentTime - lastUpdateTime > 100 || progress == pairs.size()) {
                    lastUpdateTime = currentTime;
                    new Handler(Looper.getMainLooper()).post(() -> {
                        progressBar.setProgress(progress);
                        tvStatus.setText("Repacking (" + progress + " / " + pairs.size() + ")\n" + pair.baseName + ".ast");
                    });
                }

                File originalAstFile = new File(pair.astPath);
                File parentDir = originalAstFile.getParentFile();

                File newDir = new File(parentDir, "_new");
                if (!newDir.exists()) {
                    newDir.mkdirs();
                }

                String outPath = new File(newDir, originalAstFile.getName()).getAbsolutePath();

                int result = nativeLib.repackAst(pair.astPath, pair.txtPath, outPath, targetLang);
                if (result == 1) {
                    successCount++;
                } else if (result == 0) {
                    mismatchCount++;
                }
            }

            int finalSuccess = successCount;
            int finalMismatch = mismatchCount;

            new Handler(Looper.getMainLooper()).post(() -> {
                progressDialog.dismiss();

                String msg = "Proses Selesai!\nBerhasil: " + finalSuccess + " file.";
                if (finalMismatch > 0) {
                    msg += "\nGagal (Baris TXT tidak sama dengan AST): " + finalMismatch + " file.";
                }

                new AlertDialog.Builder(context)
                        .setTitle("Hasil Repack")
                        .setMessage(msg)
                        .setPositiveButton("OK", null)
                        .show();
            });
        }).start();
    }
}