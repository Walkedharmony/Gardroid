package com.zeronovel.gardroid.ui.explorer.managers;

import android.app.AlertDialog;
import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import com.zeronovel.gardroid.R;
import com.zeronovel.gardroid.data.model.FileModel;
import com.zeronovel.gardroid.ui.explorer.ExplorerFragment;
import com.zeronovel.gardroid.ui.explorer.ExplorerViewModel;
import com.zeronovel.gardroid.bridge.NativeLib;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import android.graphics.Bitmap;
import com.zeronovel.gardroid.utils.SettingsConfig;
import java.util.HashSet;

public class ExtractionManager {
    private final ExplorerFragment fragment;
    private final ExplorerViewModel viewModel;
    private final NativeLib nativeLib;

    public void performExtraction(List<String> internalPaths, boolean convertAst, String astLanguage,
            boolean convertScn, boolean convertKs) {
        if (internalPaths.isEmpty())
            return;

        if (viewModel.currentArchiveFile == null) {
            showToast("Error: Tidak ada arsip yang aktif.");
            return;
        }

        File parentDir = viewModel.currentArchiveFile.getParentFile();
        String folderName = viewModel.currentArchiveFile.getName().replaceFirst("[.][^.]+$", "");
        File targetDir = new File(parentDir, "Gardroid_Extracted/" + folderName);
        if (!targetDir.exists())
            targetDir.mkdirs();
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.requireContext());
        builder.setTitle("Extracting...");
        builder.setCancelable(false);
        android.widget.LinearLayout layout = new android.widget.LinearLayout(fragment.requireContext());
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 10);
        final android.widget.ProgressBar progressBar = new android.widget.ProgressBar(fragment.requireContext(), null,
                android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(internalPaths.size());
        layout.addView(progressBar);

        final android.widget.TextView tvStatus = new android.widget.TextView(fragment.requireContext());
        tvStatus.setText("Initializing...");
        tvStatus.setPadding(0, 20, 0, 0);
        layout.addView(tvStatus);

        builder.setView(layout);
        AlertDialog progressDialog = builder.create();
        progressDialog.show();

        new Thread(() -> {
            long parserPointer = nativeLib.initParser(viewModel.currentArchiveFile.getAbsolutePath());

            if (parserPointer == 0) {
                fragment.requireActivity().runOnUiThread(() -> {
                    progressDialog.dismiss();
                    showToast("Gagal init parser. File arsip mungkin rusak atau tidak didukung.");
                });
                return;
            }

            int successCount = 0;

            long lastUpdateTime = 0;
            for (int i = 0; i < internalPaths.size(); i++) {
                String internalPath = internalPaths.get(i);
                int progress = i + 1;

                long currentTime = System.currentTimeMillis();
                if (currentTime - lastUpdateTime > 100 || progress == internalPaths.size()) {
                    lastUpdateTime = currentTime;
                    fragment.requireActivity().runOnUiThread(() -> {
                        progressBar.setProgress(progress);
                        tvStatus.setText(
                                "Extracting (" + progress + " / " + internalPaths.size() + ")\n" + internalPath);
                    });
                }

                File destFile = new File(targetDir, internalPath);

                if (destFile.getParentFile() != null && !destFile.getParentFile().exists()) {
                    destFile.getParentFile().mkdirs();
                }

                boolean isTlg = internalPath.toLowerCase().endsWith(".tlg");
                boolean convertTlg = fragment.settingsConfig != null
                        && fragment.settingsConfig.isManagerEnabled(SettingsConfig.KEY_TLG_CONVERT_PNG);
                boolean ok = false;

                if (isTlg && convertTlg) {
                    String pngPath = destFile.getAbsolutePath().substring(0,
                            destFile.getAbsolutePath().lastIndexOf(".")) + ".png";
                    try {
                        int[] rawData = nativeLib.getTlgPreview(viewModel.currentArchiveFile.getAbsolutePath(),
                                internalPath);
                        if (rawData != null && rawData.length > 2) {
                            int width = rawData[0];
                            int height = rawData[1];
                            Bitmap bitmap = Bitmap.createBitmap(rawData, 2, width, width, height,
                                    Bitmap.Config.ARGB_8888);
                            File pngFile = new File(pngPath);
                            if (pngFile.getParentFile() != null && !pngFile.getParentFile().exists()) {
                                pngFile.getParentFile().mkdirs();
                            }
                            try (FileOutputStream out = new FileOutputStream(pngFile)) {
                                bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
                                ok = true;
                            }
                            bitmap.recycle();
                        } else {
                            ok = nativeLib.extractFileFromPointer(parserPointer, internalPath,
                                    destFile.getAbsolutePath());
                        }
                    } catch (Exception | OutOfMemoryError e) {
                        e.printStackTrace();
                        ok = nativeLib.extractFileFromPointer(parserPointer, internalPath, destFile.getAbsolutePath());
                    }
                } else {
                    ok = nativeLib.extractFileFromPointer(parserPointer, internalPath, destFile.getAbsolutePath());
                }

                if (ok) {
                    successCount++;
                    String destPath = destFile.getAbsolutePath();
                    String lowerPath = destPath.toLowerCase();
                    if (convertAst && lowerPath.endsWith(".ast") && astLanguage != null) {
                        fragment.astManager.processAstFile(destPath, astLanguage);
                    } else if (convertScn && lowerPath.endsWith(".scn")) {
                        fragment.scnManager.processScnFile(destPath);
                    } else if (convertKs && lowerPath.endsWith(".ks")) {
                        fragment.ksManager.processKsFile(destPath);
                    }
                }
            }

            nativeLib.closeParser(parserPointer);

            int finalSuccess = successCount;

            fragment.requireActivity().runOnUiThread(() -> {
                progressDialog.dismiss();
                showToast("Selesai! " + finalSuccess + " file tersimpan di:\n" + targetDir.getAbsolutePath());

                if (fragment.adapter != null) {
                    fragment.adapter.setSelectionMode(false);
                    // loadDirectory(viewModel.currentVirtualPath);
                }
            });

        }).start();
    }

    public ExtractionManager(ExplorerFragment fragment, ExplorerViewModel viewModel, NativeLib nativeLib) {
        this.fragment = fragment;
        this.viewModel = viewModel;
        this.nativeLib = nativeLib;
    }

    public void showToast(String message) {
        if (fragment.getActivity() != null) {
            fragment.getActivity()
                    .runOnUiThread(() -> Toast.makeText(fragment.requireContext(), message, Toast.LENGTH_SHORT).show());
        }
    }

    public void showExtractionOptions() {

        List<FileModel> selected = fragment.adapter.getSelectedFiles();
        boolean hasFolderSelected = false;
        boolean hasArchiveSelected = false;
        for (FileModel f : selected) {
            if (f.isDirectory) {
                hasFolderSelected = true;
            } else if (f.isVirtual) {
                hasArchiveSelected = true;
            }
        }

        List<String> optionsList = new ArrayList<>();
        if (viewModel.isInArchiveMode) {
            optionsList.add("Extract Selected Files");
            optionsList.add("Extract ALL Files");
        } else {
            if (hasFolderSelected) {
                optionsList.add("Repack Selected Folder");
            }
            if (hasArchiveSelected) {
                optionsList.add("Extract all files on this archive");
            }
        }

        if (optionsList.isEmpty()) {
            showToast("Tidak ada aksi tersedia.");
            return;
        }

        String[] options = optionsList.toArray(new String[0]);
        new com.google.android.material.dialog.MaterialAlertDialogBuilder(fragment.requireContext(),
                com.google.android.material.R.style.ThemeOverlay_Material3_MaterialAlertDialog)
                .setTitle("Batch Actions")
                .setIcon(R.drawable.ic_pack)
                .setItems(options, (dialog, which) -> {
                    String choice = options[which];
                    if (choice.contains("Extract Selected")) {
                        handleExtractSelected(selected);
                    } else if (choice.contains("Extract ALL")) {
                        handleExtractAll();
                    } else if (choice.contains("Extract all files on this archive")) {
                        for (FileModel f : selected) {
                            if (f.isVirtual) {
                                handleExtractAllOffline(f.file);
                            }
                        }
                    } else if (choice.contains("Repack")) {
                        for (FileModel f : selected) {
                            if (f.isDirectory) {
                                fragment.repackManager.showRepackFormatDialog(f);
                                break;
                            }
                        }
                    }
                })
                .show();
    }

    public void handleExtractSelected(List<FileModel> selected) {
        if (selected.isEmpty()) {
            showToast("Pilih file atau folder dulu!");
            return;
        }

        Set<String> uniquePathsToExtract = new HashSet<>();

        for (FileModel f : selected) {
            String fullItemPath = viewModel.currentVirtualPath + f.name;
            if (!f.isDirectory) {
                uniquePathsToExtract.add(fullItemPath);
            } else {
                String folderPrefix = fullItemPath + "/";
                for (String entry : viewModel.rawArchiveData) {
                    if (entry.startsWith(folderPrefix)) {
                        uniquePathsToExtract.add(entry.split("\\|")[0]);
                    }
                }
            }
        }
        List<String> finalPaths = new ArrayList<>(uniquePathsToExtract);

        if (fragment.astManager.containsAstFiles(finalPaths)) {
            fragment.astManager.configureAstExtraction(viewModel.currentArchiveFile, finalPaths,
                    (convertAst, language) -> {

                        checkScriptsAndExtract(finalPaths, convertAst, language);
                    });
        } else {

            checkScriptsAndExtract(finalPaths, false, null);
        }
    }

    public void handleExtractAllOffline(File archiveFile) {
        new Thread(() -> {
            String[] rawDataArray = nativeLib.getArchiveFileList(archiveFile.getAbsolutePath());
            if (rawDataArray == null || rawDataArray.length == 0) {
                fragment.requireActivity().runOnUiThread(() -> showToast("Gagal membaca arsip atau arsip kosong."));
                return;
            }
            List<String> allPaths = new ArrayList<>();
            for (String entry : rawDataArray) {
                allPaths.add(entry.split("\\|")[0]);
            }
            fragment.requireActivity().runOnUiThread(() -> {
                performOfflineExtraction(archiveFile, allPaths);
            });
        }).start();
    }

    public void performOfflineExtraction(File archiveFile, List<String> internalPaths) {
        if (internalPaths.isEmpty())
            return;

        File parentDir = archiveFile.getParentFile();
        String folderName = archiveFile.getName().replaceFirst("[.][^.]+$", "");
        File targetDir = new File(parentDir, "Gardroid_Extracted/" + folderName);
        if (!targetDir.exists())
            targetDir.mkdirs();

        android.app.AlertDialog.Builder builder = new android.app.AlertDialog.Builder(fragment.requireContext());
        builder.setTitle("Extracting...");
        builder.setCancelable(false);
        android.widget.LinearLayout layout = new android.widget.LinearLayout(fragment.requireContext());
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 10);

        final android.widget.ProgressBar progressBar = new android.widget.ProgressBar(fragment.requireContext(), null,
                android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(internalPaths.size());
        layout.addView(progressBar);

        final android.widget.TextView tvStatus = new android.widget.TextView(fragment.requireContext());
        tvStatus.setText("Initializing...");
        tvStatus.setPadding(0, 20, 0, 0);
        layout.addView(tvStatus);

        builder.setView(layout);
        android.app.AlertDialog progressDialog = builder.create();
        progressDialog.show();

        new Thread(() -> {
            long parserPointer = nativeLib.initParser(archiveFile.getAbsolutePath());
            if (parserPointer == 0) {
                fragment.requireActivity().runOnUiThread(() -> {
                    progressDialog.dismiss();
                    showToast("Gagal inisialisasi arsip.");
                });
                return;
            }

            int successCount = 0;
            int current = 0;
            for (String internalPath : internalPaths) {
                current++;
                final int currentProgress = current;
                final String currentFile = internalPath;
                fragment.requireActivity().runOnUiThread(() -> {
                    progressBar.setProgress(currentProgress);
                    tvStatus.setText(currentProgress + "/" + internalPaths.size() + "\n" + currentFile);
                });

                File destFile = new File(targetDir, internalPath);
                if (destFile.getParentFile() != null && !destFile.getParentFile().exists()) {
                    destFile.getParentFile().mkdirs();
                }

                boolean ok = nativeLib.extractFileFromPointer(parserPointer, internalPath, destFile.getAbsolutePath());
                if (ok)
                    successCount++;
            }

            nativeLib.closeParser(parserPointer);

            final int finalSuccess = successCount;
            fragment.requireActivity().runOnUiThread(() -> {
                progressDialog.dismiss();
                showToast("Extract Selesai! Berhasil: " + finalSuccess + "/" + internalPaths.size());
                if (fragment.adapter != null) {
                    fragment.adapter.setSelectionMode(false);
                }
            });
        }).start();
    }

    public void handleExtractAll() {
        List<String> allPaths = new ArrayList<>();
        for (String entry : viewModel.rawArchiveData) {
            allPaths.add(entry.split("\\|")[0]);
        }

        if (fragment.astManager.containsAstFiles(allPaths)) {

            fragment.astManager.configureAstExtraction(viewModel.currentArchiveFile, allPaths,
                    (convertAst, language) -> {
                        checkScriptsAndExtract(allPaths, convertAst, language);
                    });
        } else {

            checkScriptsAndExtract(allPaths, false, null);
        }
    }

    public void checkScriptsAndExtract(List<String> paths, boolean convertAst, String astLang) {
        // 1. Cek SCN
        if (fragment.scnManager.containsScnFiles(paths)) {
            fragment.scnManager.configureScnExtraction(paths, (convertScn) -> {
                checkKsAndExtract(paths, convertAst, astLang, convertScn);
            });
        } else {
            checkKsAndExtract(paths, convertAst, astLang, false);
        }
    }

    public void checkKsAndExtract(List<String> paths, boolean convertAst, String astLang, boolean convertScn) {
        // 2. Cek KS
        if (fragment.ksManager.containsKsFiles(paths)) {
            fragment.ksManager.configureKsExtraction(paths, (convertKs) -> {
                performExtraction(paths, convertAst, astLang, convertScn, convertKs);
            });
        } else {
            performExtraction(paths, convertAst, astLang, convertScn, false);
        }
    }

    public void performExtraction(List<String> internalPaths) {
        performExtraction(internalPaths, false, null, false, false);
    }

}
