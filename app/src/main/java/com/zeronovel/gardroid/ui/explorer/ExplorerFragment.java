package com.zeronovel.gardroid.ui.explorer;

import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.widget.ImageView;
import android.graphics.Color;
import android.widget.TextView;
import android.widget.ScrollView;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.lifecycle.ViewModelProvider;

import com.zeronovel.gardroid.R;
import com.zeronovel.gardroid.data.model.FileModel;
import com.zeronovel.gardroid.bridge.NativeLib;
import com.zeronovel.gardroid.databinding.FragmentExplorerBinding;
import com.zeronovel.gardroid.ui.base.BaseFragment;
import com.zeronovel.gardroid.utils.AstManager;
import com.zeronovel.gardroid.utils.AstPackerManager;
import com.zeronovel.gardroid.utils.KsManager;
import com.zeronovel.gardroid.utils.KsPackerManager;
import com.zeronovel.gardroid.utils.ScnManager;
import com.zeronovel.gardroid.utils.SettingsConfig;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import com.zeronovel.gardroid.ui.explorer.managers.PreviewManager;
import com.zeronovel.gardroid.ui.explorer.managers.ExtractionManager;
import com.zeronovel.gardroid.ui.explorer.managers.RepackManager;
import java.util.Set;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

public class ExplorerFragment extends BaseFragment<FragmentExplorerBinding>
        implements ExplorerAdapter.OnFileClickListener {

    public ExplorerAdapter adapter;
    private PreviewManager previewManager;
    public ExtractionManager extractionManager;
    public RepackManager repackManager;
    private final NativeLib nativeLib = new NativeLib();
    public AstManager astManager;
    public ScnManager scnManager;
    public KsManager ksManager;
    private KsPackerManager ksPackerManager;

    private AstPackerManager astPackerManager;
    public SettingsConfig settingsConfig;
    private ExplorerViewModel viewModel;

    @Override
    protected FragmentExplorerBinding inflateBinding(LayoutInflater inflater, ViewGroup container) {
        return FragmentExplorerBinding.inflate(inflater, container, false);
    }

    @Override
    protected void setupViews() {
        viewModel = new ViewModelProvider(this).get(ExplorerViewModel.class);

        // Inisialisasi Manager
        astManager = new AstManager(requireContext());
        scnManager = new ScnManager(requireContext());
        astPackerManager = new AstPackerManager(requireContext());
        ksManager = new KsManager(requireContext());
        ksPackerManager = new KsPackerManager(requireContext());

        getBinding().drawerLayout.setDrawerLockMode(androidx.drawerlayout.widget.DrawerLayout.LOCK_MODE_LOCKED_CLOSED);

        // --- SETUP SETTINGS BUTTON ---3
        getBinding().btnSettings.setOnClickListener(v -> {
            if (!getBinding().drawerLayout.isDrawerOpen(androidx.core.view.GravityCompat.END)) {
                getBinding().drawerLayout.openDrawer(androidx.core.view.GravityCompat.END);
            } else {
                getBinding().drawerLayout.closeDrawer(androidx.core.view.GravityCompat.END);
            }
        });

        // --- SETUP DRAWER TOGGLES ---
        settingsConfig = new SettingsConfig(requireContext());

        getBinding().switchScnManager.setChecked(settingsConfig.isManagerEnabled(SettingsConfig.KEY_SCN_MANAGER));
        getBinding().switchKsManager.setChecked(settingsConfig.isManagerEnabled(SettingsConfig.KEY_KS_MANAGER));
        getBinding().switchAstManager.setChecked(settingsConfig.isManagerEnabled(SettingsConfig.KEY_AST_MANAGER));
        getBinding().switchTlgPng.setChecked(settingsConfig.isManagerEnabled(SettingsConfig.KEY_TLG_CONVERT_PNG));

        getBinding().switchScnManager.setOnCheckedChangeListener((buttonView, isChecked) -> {
            settingsConfig.setManagerEnabled(SettingsConfig.KEY_SCN_MANAGER, isChecked);
        });
        getBinding().switchKsManager.setOnCheckedChangeListener((buttonView, isChecked) -> {
            settingsConfig.setManagerEnabled(SettingsConfig.KEY_KS_MANAGER, isChecked);
        });
        getBinding().switchAstManager.setOnCheckedChangeListener((buttonView, isChecked) -> {
            settingsConfig.setManagerEnabled(SettingsConfig.KEY_AST_MANAGER, isChecked);
        });
        getBinding().switchTlgPng.setOnCheckedChangeListener((buttonView, isChecked) -> {
            settingsConfig.setManagerEnabled(SettingsConfig.KEY_TLG_CONVERT_PNG, isChecked);
        });

        // --- SETUP DRAWER TOGGLES ---
        getBinding().drawerLayout.setDrawerLockMode(androidx.drawerlayout.widget.DrawerLayout.LOCK_MODE_LOCKED_CLOSED);

        getBinding().btnHelpScn.setOnClickListener(v -> showHelpDialog("SCN Manager",
                "OFF : Menonaktifkan konversi otomatis pada archive yang memiliki file skenario .scn pada saat ekstraksi menyeluruh ataupun pada seleksi file\n\n"
                        +
                        "ON : Mengaktifkan pencarian otomatis dan konversi otomatis pada archive yang memiliki file skenario .scn pada saat ekstraksi menyeluruh ataupun pada seleksi file"));

        getBinding().btnHelpKs.setOnClickListener(v -> showHelpDialog("KS Manager",
                "OFF : Menonaktifkan konversi otomatis pada archive yang memiliki file skenario .ks pada saat ekstraksi menyeluruh ataupun pada seleksi file\n\n"
                        +
                        "ON : Mengaktifkan pencarian otomatis dan konversi otomatis pada archive yang memiliki file skenario .ks pada saat ekstraksi menyeluruh ataupun pada seleksi file"));

        getBinding().btnHelpAst.setOnClickListener(v -> showHelpDialog("AST Manager",
                "OFF : Menonaktifkan konversi otomatis pada archive yang memiliki file skenario .ast pada saat ekstraksi menyeluruh ataupun pada seleksi file\n\n"
                        +
                        "ON : Mengaktifkan pencarian otomatis dan konversi otomatis pada archive yang memiliki file skenario .ast pada saat ekstraksi menyeluruh ataupun pada seleksi file"));

        getBinding().btnHelpTlg.setOnClickListener(v -> showHelpDialog("TLG Auto Convert",
                "OFF : Mengekstrak file gambar TLG secara mentah (Raw) apa adanya.\n\n" +
                        "ON : Secara otomatis mendekode file TLG ke dalam memori dan mengonversinya menjadi PNG (tanpa mengompresi kualitas) selama proses pengekstrakan. File TLG mentah hasil ekstraksi akan otomatis dihapus."));

        extractionManager = new ExtractionManager(this, viewModel, nativeLib);
        repackManager = new RepackManager(this, viewModel, nativeLib);
        previewManager = new PreviewManager(this, viewModel, nativeLib);
        adapter = new ExplorerAdapter(this);
        getBinding().rvFiles.setLayoutManager(new LinearLayoutManager(getContext()));
        getBinding().rvFiles.setAdapter(adapter);

        getBinding().fabAction.setOnClickListener(v -> {
            if ((Boolean.TRUE.equals(viewModel.isAstPackerMode.getValue()))) {

                handleAstPackerAction();
            } else {

                extractionManager.showExtractionOptions();
            }
        });

        requireActivity().getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {

                if (getBinding().drawerLayout.isDrawerOpen(androidx.core.view.GravityCompat.END)) {
                    getBinding().drawerLayout.closeDrawer(androidx.core.view.GravityCompat.END);
                    return;
                }

                if (adapter.isSelectionMode) {
                    adapter.setSelectionMode(false);
                    return;
                }

                if (viewModel.isInArchiveMode) {
                    if (!viewModel.currentVirtualPath.isEmpty()) {
                        navigateUpVirtual();
                    } else {
                        exitArchiveMode();
                    }
                }

                else if (viewModel.currentDirectory != null && viewModel.currentDirectory.getParentFile() != null &&
                        viewModel.currentDirectory.getAbsolutePath()
                                .startsWith(Environment.getExternalStorageDirectory().getAbsolutePath())) {
                    loadDirectory(viewModel.currentDirectory.getParentFile());
                }

                else {
                    setEnabled(false);
                    requireActivity().getOnBackPressedDispatcher().onBackPressed();
                }
            }
        });
    }

    private void handleAstPackerAction() {
        if (viewModel.isInArchiveMode) {
            showToast("AST Packer hanya bisa digunakan di luar arsip (folder penyimpanan lokal).");
            return;
        }

        List<FileModel> selected = adapter.getSelectedFiles();
        if (selected.isEmpty()) {
            showToast("Pilih folder atau pasangkan file .ast & .txt terlebih dahulu!");
            return;
        }

        List<String> paths = new ArrayList<>();
        for (FileModel f : selected) {
            if (f.file != null) {
                paths.add(f.file.getAbsolutePath());
            }
        }

        astPackerManager.processSelection(paths);
        adapter.setSelectionMode(false);
    }

    private void showHelpDialog(String title, String message) {
        new MaterialAlertDialogBuilder(requireContext(),
                com.google.android.material.R.style.ThemeOverlay_Material3_MaterialAlertDialog)
                .setTitle(title)
                .setMessage(message)
                .setPositiveButton("Mengerti", null)
                .setIcon(R.drawable.ic_help)
                .show();
    }

    @Override
    protected void observeData() {
        viewModel.fileList.observe(getViewLifecycleOwner(), models -> adapter.updateList(models));
        viewModel.currentPathText.observe(getViewLifecycleOwner(), this::updateBreadcrumbs);
        viewModel.isEmpty.observe(getViewLifecycleOwner(), this::updateEmptyState);
        checkPermissionAndLoad();
    }

    private int getThemeColor(int attrResId) {
        android.util.TypedValue typedValue = new android.util.TypedValue();
        requireContext().getTheme().resolveAttribute(attrResId, typedValue, true);
        return typedValue.data;
    }

    private void updateBreadcrumbs(String path) {
        android.widget.LinearLayout layoutBreadcrumb = getBinding().layoutBreadcrumb;
        layoutBreadcrumb.removeAllViews();

        if (path == null || path.isEmpty())
            return;

        String[] segments = path.split("/");
        String currentBuildPath = "";

        for (int i = 0; i < segments.length; i++) {
            String segment = segments[i];
            if (segment.isEmpty())
                continue;

            currentBuildPath += "/" + segment;
            final String targetPath = currentBuildPath;

            View breadcrumbView = getLayoutInflater().inflate(R.layout.item_breadcrumb, layoutBreadcrumb, false);
            android.widget.TextView tvTitle = breadcrumbView.findViewById(R.id.tvTitle);
            android.widget.TextView tvSeparator = breadcrumbView.findViewById(R.id.tvSeparator);
            android.widget.ImageView ivIcon = breadcrumbView.findViewById(R.id.ivIcon);
            com.google.android.material.card.MaterialCardView cardChip = breadcrumbView.findViewById(R.id.cardChip);

            tvTitle.setText(segment);

            if (layoutBreadcrumb.getChildCount() == 0) {
                tvSeparator.setVisibility(View.GONE);
                if (segment.equalsIgnoreCase("storage") || segment.equalsIgnoreCase("emulated")) {
                    ivIcon.setVisibility(View.VISIBLE);
                    ivIcon.setImageResource(R.drawable.ic_folder);
                }
            } else {
                tvSeparator.setVisibility(View.VISIBLE);
            }

            boolean isLast = (i == segments.length - 1);
            boolean isSafeRoot = segment.equalsIgnoreCase("storage") || segment.equalsIgnoreCase("emulated");

            if (isLast) {
                if (viewModel.isInArchiveMode) {
                    cardChip.setCardBackgroundColor(getThemeColor(R.attr.colorArchiveBg));
                    tvTitle.setTextColor(getThemeColor(R.attr.colorArchiveText));
                    ivIcon.setVisibility(View.VISIBLE);
                    ivIcon.setImageResource(R.drawable.ic_archive);
                    ivIcon.setColorFilter(getThemeColor(R.attr.colorArchiveText));
                } else {

                    if (isSafeRoot) {
                        cardChip.setCardBackgroundColor(android.graphics.Color.parseColor("#FFE5E5"));
                        tvTitle.setTextColor(android.graphics.Color.parseColor("#D32F2F"));
                        if (ivIcon.getVisibility() == View.VISIBLE)
                            ivIcon.setColorFilter(android.graphics.Color.parseColor("#D32F2F"));
                    } else {
                        cardChip.setCardBackgroundColor(
                                getThemeColor(com.google.android.material.R.attr.colorPrimaryContainer));
                        tvTitle.setTextColor(getThemeColor(com.google.android.material.R.attr.colorOnPrimaryContainer));
                    }
                }
            } else {
                if (isSafeRoot) {
                    tvTitle.setTextColor(android.graphics.Color.parseColor("#D32F2F"));
                    if (ivIcon.getVisibility() == View.VISIBLE)
                        ivIcon.setColorFilter(android.graphics.Color.parseColor("#D32F2F"));
                }
            }

            cardChip.setOnClickListener(v -> {

                if (viewModel.isInArchiveMode && viewModel.currentArchiveFile != null
                        && targetPath.equals(viewModel.currentArchiveFile.getAbsolutePath())) {

                    exitArchiveMode();
                    return;
                }

                if (viewModel.isInArchiveMode && viewModel.currentArchiveFile != null
                        && targetPath.contains(viewModel.currentArchiveFile.getAbsolutePath())) {
                    String virtualPath = targetPath.replace(viewModel.currentArchiveFile.getAbsolutePath(), "");
                    if (virtualPath.startsWith("/"))
                        virtualPath = virtualPath.substring(1);
                    if (!virtualPath.endsWith("/") && !virtualPath.isEmpty())
                        virtualPath += "/";
                    viewModel.currentVirtualPath = virtualPath;
                    viewModel.refreshVirtualView();
                } else {
                    if (viewModel.isInArchiveMode) {
                        exitArchiveMode();
                    }
                    loadDirectory(new java.io.File(targetPath));
                }
            });

            layoutBreadcrumb.addView(breadcrumbView);
        }

        getBinding().scrollPath.post(() -> {
            getBinding().scrollPath.fullScroll(android.widget.HorizontalScrollView.FOCUS_RIGHT);
        });
    }

    private void checkPermissionAndLoad() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (Environment.isExternalStorageManager())
                loadInitialDirectory();
            else {
                try {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.addCategory("android.intent.category.DEFAULT");
                    intent.setData(Uri.parse(String.format("package:%s", requireContext().getPackageName())));
                    startActivityForResult(intent, 100);
                } catch (Exception e) {
                    Intent intent = new Intent();
                    intent.setAction(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivityForResult(intent, 100);
                }
            }
        } else {
            loadInitialDirectory();
        }
    }

    private void loadInitialDirectory() {
        android.content.SharedPreferences prefs = requireContext().getSharedPreferences("gardroid_prefs",
                android.content.Context.MODE_PRIVATE);
        String lastPath = prefs.getString("last_path", Environment.getExternalStorageDirectory().getAbsolutePath());
        File dir = new File(lastPath);
        if (dir.exists() && dir.isDirectory()) {
            loadDirectory(dir);
        } else {
            loadDirectory(Environment.getExternalStorageDirectory());
        }
    }

    private void updateEmptyState(boolean isEmpty) {
        if (isEmpty) {
            getBinding().layoutEmpty.setVisibility(View.VISIBLE);
            getBinding().rvFiles.setVisibility(View.GONE);
        } else {
            getBinding().layoutEmpty.setVisibility(View.GONE);
            getBinding().rvFiles.setVisibility(View.VISIBLE);
        }
    }

    public void loadDirectory(File directory) {
        viewModel.loadDirectory(directory);
        if (!viewModel.isInArchiveMode) {
            android.content.SharedPreferences prefs = requireContext().getSharedPreferences("gardroid_prefs",
                    android.content.Context.MODE_PRIVATE);
            prefs.edit().putString("last_path", directory.getAbsolutePath()).apply();
        }
    }

    private void enterArchiveMode(File archiveFile) {
        if (!viewModel.enterArchiveMode(archiveFile)) {
            showToast("Gagal membuka arsip (Format tidak didukung/Encrypted)");
        }
    }

    private void navigateUpVirtual() {
        viewModel.navigateUpVirtual();
    }

    private void exitArchiveMode() {
        viewModel.exitArchiveMode();
    }

    private void refreshVirtualView() {
        viewModel.refreshVirtualView();
    }

    private boolean isSupportedArchive(String fileName) {
        String lower = fileName.toLowerCase();
        return lower.endsWith(".xp3") || lower.endsWith(".pfs") || lower.endsWith(".pfs.000")
                || lower.endsWith(".pfs.001") || lower.endsWith(".arc");
    }

    public void performExtraction(List<String> selectedPaths) {
        if (extractionManager != null) {
            extractionManager.performExtraction(selectedPaths);
        }
    }

    public void showRepackFormatDialog(FileModel folder) {
        if (repackManager != null) {
            repackManager.showRepackFormatDialog(folder);
        }
    }

    @Override
    public void onFileClick(FileModel file) {
        if (adapter.isSelectionMode)
            return;
        if (viewModel.isInArchiveMode) {
            if (file.isDirectory) {
                viewModel.currentVirtualPath += file.name + "/";
                refreshVirtualView();
            } else {
                String name = file.name.toLowerCase();
                if (name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".bmp") || name.endsWith(".tlg") ||
                        name.endsWith(".tjs") || name.endsWith(".ks") || name.endsWith(".ini") || name.endsWith(".lua")
                        ||
                        name.endsWith(".txt") || name.endsWith(".bgi") || name.endsWith(".ogg")
                        || name.endsWith(".opus")) {
                    previewManager.showModernPreview(file, viewModel.isInArchiveMode);
                } else {
                    previewManager.checkAndShowPreview(file);
                }
            }
        } else {
            if (file.isDirectory) {
                loadDirectory(file.file);
            } else if (file.isXp3 || isSupportedArchive(file.name)) {
                enterArchiveMode(file.file);
            } else {
                String name = file.name.toLowerCase();
                if (name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".bmp") || name.endsWith(".tlg") ||
                        name.endsWith(".tjs") || name.endsWith(".ks") || name.endsWith(".ini") || name.endsWith(".lua")
                        ||
                        name.endsWith(".txt") || name.endsWith(".bgi") || name.endsWith(".ogg")
                        || name.endsWith(".opus")) {
                    previewManager.showModernPreview(file, viewModel.isInArchiveMode);
                } else {
                    showToast("File System: " + file.name);
                }
            }
        }
    }

    @Override
    public void onSelectionChanged(int count) {
        if (adapter.isSelectionMode) {
            if (count > 0) {
                getBinding().fabAction.setVisibility(View.VISIBLE);

                boolean onlyArchivesSelected = true;
                List<FileModel> selected = adapter.getSelectedFiles();
                for (FileModel f : selected) {
                    if (f.isDirectory || !f.name.toLowerCase().endsWith(".xp3")) {
                        onlyArchivesSelected = false;
                        break;
                    }
                }

                if (viewModel.isInArchiveMode) {
                    getBinding().fabAction.setIconResource(R.drawable.ic_extract);
                    getBinding().fabAction.setText("Ekstrak " + count + " Item");
                } else if (onlyArchivesSelected) {
                    getBinding().fabAction.setIconResource(R.drawable.ic_extract);
                    getBinding().fabAction.setText("Ekstrak " + count + " Archive");
                } else {
                    getBinding().fabAction.setIconResource(R.drawable.ic_pack);
                    getBinding().fabAction.setText("Pack " + count + " Item");
                }
            } else {
                getBinding().fabAction.setVisibility(View.GONE);
            }
        } else {
            getBinding().fabAction.setVisibility(View.GONE);
        }
    }

    @Override
    public void onMoreClick(FileModel file) {
        List<String> options = new ArrayList<>();
        String title = "Options: " + file.name;
        if (file.isDirectory && !viewModel.isInArchiveMode) {
            options.add("Repack to Archive");
            options.add("Repack AST in this Folder");
            options.add("Repack KS in this Folder");
        } else if (viewModel.isInArchiveMode && !file.isDirectory) {
            options.add("Extract File");
        } else if (!viewModel.isInArchiveMode && file.isVirtual) {
            options.add("Extract all files on this archive");
        }

        if (options.isEmpty()) {
            showToast("Tidak ada opsi khusus untuk file/folder ini.");
            return;
        }

        new AlertDialog.Builder(requireContext())
                .setTitle(title)
                .setItems(options.toArray(new String[0]), (dialog, which) -> {
                    String selected = options.get(which);

                    if (selected.equals("Repack to Archive")) {
                        showRepackFormatDialog(file);
                    } else if (selected.equals("Repack AST in this Folder")) {
                        List<String> folderToProcess = Collections.singletonList(file.file.getAbsolutePath());
                        astPackerManager.processSelection(folderToProcess);
                    } else if (selected.equals("Repack KS in this Folder")) {
                        List<String> folderToProcess = Collections.singletonList(file.file.getAbsolutePath());
                        ksPackerManager.processSelection(folderToProcess);
                    } else if (selected.equals("Extract File")) {
                        List<String> singlePath = Collections.singletonList(viewModel.currentVirtualPath + file.name);
                        performExtraction(singlePath);
                    } else if (selected.equals("Extract all files on this archive")) {
                        extractionManager.handleExtractAllOffline(file.file);
                    }

                })
                .show();
    }

}