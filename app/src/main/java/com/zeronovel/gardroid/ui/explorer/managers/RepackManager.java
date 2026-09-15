package com.zeronovel.gardroid.ui.explorer.managers;

import android.app.AlertDialog;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.zeronovel.gardroid.R;
import com.zeronovel.gardroid.bridge.NativeLib;
import com.zeronovel.gardroid.data.model.FileModel;
import com.zeronovel.gardroid.ui.explorer.ExplorerFragment;
import com.zeronovel.gardroid.ui.explorer.ExplorerViewModel;

import java.io.File;
import java.util.Collections;
import java.util.List;

public class RepackManager {

    private final ExplorerFragment fragment;
    private final ExplorerViewModel viewModel;
    private final NativeLib nativeLib;

    public RepackManager(ExplorerFragment fragment, ExplorerViewModel viewModel, NativeLib nativeLib) {
        this.fragment = fragment;
        this.viewModel = viewModel;
        this.nativeLib = nativeLib;
    }

    private void showToast(String message) {
        if (fragment.getContext() != null) {
            fragment.requireActivity().runOnUiThread(() -> 
                Toast.makeText(fragment.getContext(), message, Toast.LENGTH_SHORT).show()
            );
        }
    }

    public void showRepackFormatDialog(FileModel folder) {
        String[] formats = { "Repack to .xp3 (Kirikiri)", "Repack to .pfs (Artemis)" };
        new com.google.android.material.dialog.MaterialAlertDialogBuilder(fragment.requireContext(),
                com.google.android.material.R.style.ThemeOverlay_Material3_MaterialAlertDialog)
                .setTitle("Pilih Format Arsip")
                .setIcon(R.drawable.ic_archive)
                .setItems(formats, (dialog, which) -> {
                    if (which == 0) {
                        performRepack(Collections.singletonList(folder), "xp3");
                    } else if (which == 1) {
                        performRepack(Collections.singletonList(folder), "pfs");
                    }
                })
                .show();
    }


    private void performRepack(List<FileModel> selectedFiles, String format) {
        FileModel targetFolder = null;
        for (FileModel f : selectedFiles) {
            if (f.isDirectory) {
                targetFolder = f;
                break;
            }
        }
        if (targetFolder == null)
            return;

        File sourceDir = targetFolder.file;
        File parentDir = sourceDir.getParentFile();
        File outputFile = new File(parentDir, sourceDir.getName() + "." + format);
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.requireContext());
        builder.setTitle("Repacking " + sourceDir.getName());
        builder.setCancelable(false);

        android.widget.LinearLayout layout = new android.widget.LinearLayout(fragment.requireContext());
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 10);

        final android.widget.ProgressBar progressBar = new android.widget.ProgressBar(fragment.requireContext(), null,
                android.R.attr.progressBarStyleHorizontal);
        progressBar.setIndeterminate(false);
        layout.addView(progressBar);

        final android.widget.TextView tvStatus = new android.widget.TextView(fragment.requireContext());
        tvStatus.setText("Scanning files...");
        layout.addView(tvStatus);

        builder.setView(layout);
        AlertDialog progressDialog = builder.create();
        progressDialog.show();

        new Thread(() -> {
            boolean success = false;

            NativeLib.RepackListener listener = (currentFile, current, total) -> {
                fragment.requireActivity().runOnUiThread(() -> {
                    if (progressBar.getMax() != total)
                        progressBar.setMax(total);
                    progressBar.setProgress(current);
                    tvStatus.setText(String.format("%s\n(%d / %d)", currentFile, current, total));
                });
            };

            if (format.equals("xp3")) {
                success = nativeLib.repackXp3(sourceDir.getAbsolutePath(), outputFile.getAbsolutePath(), listener);
            } else {
                success = nativeLib.repackPfs(sourceDir.getAbsolutePath(), outputFile.getAbsolutePath(), listener);
            }

            boolean finalSuccess = success;
            fragment.requireActivity().runOnUiThread(() -> {
                progressDialog.dismiss();
                if (finalSuccess) {
                    this.showToast("Repack " + format.toUpperCase() + " Berhasil!");
                    fragment.adapter.setSelectionMode(false);
                    fragment.loadDirectory(viewModel.currentDirectory);
                } else {
                    this.showToast("Repack Gagal.");
                }
            });
        }).start();
    }

}
