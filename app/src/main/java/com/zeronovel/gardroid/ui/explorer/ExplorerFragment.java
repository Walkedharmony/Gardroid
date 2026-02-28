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

import com.zeronovel.gardroid.data.model.FileModel;
import com.zeronovel.gardroid.bridge.NativeLib;
import com.zeronovel.gardroid.databinding.FragmentExplorerBinding;
import com.zeronovel.gardroid.ui.base.BaseFragment;
import com.zeronovel.gardroid.utils.AstManager;
import com.zeronovel.gardroid.utils.AstPackerManager;
import com.zeronovel.gardroid.utils.KsManager;
import com.zeronovel.gardroid.utils.KsPackerManager;
import com.zeronovel.gardroid.utils.ScnManager;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;


public class ExplorerFragment extends BaseFragment<FragmentExplorerBinding>
        implements ExplorerAdapter.OnFileClickListener {

    private ExplorerAdapter adapter;
    private final NativeLib nativeLib = new NativeLib();
    private AstManager astManager;
    private ScnManager scnManager;
    private KsManager ksManager;
    private KsPackerManager ksPackerManager;


    private AstPackerManager astPackerManager;
    private boolean isAstPackerMode = false;

    private File currentDirectory;
    private File currentArchiveFile = null;
    private boolean isInArchiveMode = false;

    private final List<String> rawArchiveData = new ArrayList<>();
    private String currentVirtualPath = "";

    @Override
    protected FragmentExplorerBinding inflateBinding(LayoutInflater inflater, ViewGroup container) {
        return FragmentExplorerBinding.inflate(inflater, container, false);
    }

    @Override
    protected void setupViews() {
        setupToolbar(getBinding().toolbar, "Gardroid", false);

        // Inisialisasi Manager
        astManager = new AstManager(requireContext());
        scnManager = new ScnManager(requireContext());
        astPackerManager = new AstPackerManager(requireContext()); // Tambahkan ini
        ksManager = new KsManager(requireContext());
        ksPackerManager = new KsPackerManager(requireContext());

        // --- SETUP MENU TOOLBAR (TITIK TIGA) ---
        android.view.MenuItem astMenu = getBinding().toolbar.getMenu().add(0, 101, 0, "AST Packer Mode");
        astMenu.setCheckable(true);
        astMenu.setChecked(isAstPackerMode);
        astMenu.setShowAsAction(android.view.MenuItem.SHOW_AS_ACTION_NEVER);

        getBinding().toolbar.setOnMenuItemClickListener(item -> {
            if (item.getItemId() == 101) {
                isAstPackerMode = !isAstPackerMode;
                item.setChecked(isAstPackerMode);

                String msg = isAstPackerMode ?
                        "AST Packer Mode AKTIF\nPilih file .ast & .txt lalu tekan tombol Proses." :
                        "AST Packer Mode NONAKTIF";
                Toast.makeText(requireContext(), msg, Toast.LENGTH_LONG).show();
                return true;
            }
            return false;
        });
        // ---------------------------------------

        adapter = new ExplorerAdapter(this);
        getBinding().rvFiles.setLayoutManager(new LinearLayoutManager(getContext()));
        getBinding().rvFiles.setAdapter(adapter);

        adapter.registerAdapterDataObserver(new RecyclerView.AdapterDataObserver() {
            @Override
            public void onChanged() {
                super.onChanged();
                if (adapter.isSelectionMode) {
                    getBinding().fabAction.setVisibility(View.VISIBLE);
                } else {
                    getBinding().fabAction.setVisibility(View.GONE);
                }
            }
        });

        // --- MODIFIKASI LOGIKA FAB ---
        getBinding().fabAction.setOnClickListener(v -> {
            if (isAstPackerMode) {
                // Eksekusi khusus AST Packer
                handleAstPackerAction();
            } else {
                // Eksekusi default (Extract XP3/PFS)
                showExtractionOptions();
            }
        });

        requireActivity().getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {

                if (adapter.isSelectionMode) {
                    adapter.setSelectionMode(false);
                    return;
                }

                if (isInArchiveMode) {
                    if (!currentVirtualPath.isEmpty()) {
                        navigateUpVirtual();
                    } else {
                        exitArchiveMode();
                    }
                }

                else if (currentDirectory != null && currentDirectory.getParentFile() != null &&
                        currentDirectory.getAbsolutePath().startsWith(Environment.getExternalStorageDirectory().getAbsolutePath())) {
                    loadDirectory(currentDirectory.getParentFile());
                }

                else {
                    setEnabled(false);
                    requireActivity().getOnBackPressedDispatcher().onBackPressed();
                }
            }
        });
    }

    private void handleAstPackerAction() {
        if (isInArchiveMode) {
            showToast("AST Packer hanya bisa digunakan di luar arsip (folder penyimpanan lokal).");
            return;
        }

        List<FileModel> selected = adapter.getSelectedFiles();
        if (selected.isEmpty()) {
            showToast("Pilih folder atau pasangkan file .ast & .txt terlebih dahulu!");
            return;
        }

        // Ambil path absolut dari file-file yang dipilih
        List<String> paths = new ArrayList<>();
        for (FileModel f : selected) {
            // Gunakan f.file, bukan f.getFile()
            if (f.file != null) {
                paths.add(f.file.getAbsolutePath());
            }
        }

        // Lemparkan ke AstPackerManager untuk dipasangkan (Smart Pairing) dan direpack
        astPackerManager.processSelection(paths);

        // Matikan mode seleksi setelah memanggil proses
        adapter.setSelectionMode(false);
    }

    @Override
    protected void observeData() {
        checkPermissionAndLoad();
    }

    private void checkPermissionAndLoad() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (Environment.isExternalStorageManager()) loadDirectory(Environment.getExternalStorageDirectory());
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

    private void loadDirectory(File directory) {
        this.currentDirectory = directory;
        getBinding().tvPath.setText(directory.getAbsolutePath());

        File[] files = directory.listFiles();
        List<FileModel> models = new ArrayList<>();

        if (files != null) {
            for (File file : files) {
                if (!file.isHidden()) models.add(new FileModel(file));
            }
        }

        Collections.sort(models, (o1, o2) -> {
            if (o1.isDirectory && !o2.isDirectory) return -1;
            if (!o1.isDirectory && o2.isDirectory) return 1;
            return o1.name.compareToIgnoreCase(o2.name);
        });

        adapter.updateList(models);
        updateEmptyState(models.isEmpty());
    }

    private void enterArchiveMode(File archiveFile) {

        String[] rawDataArray = nativeLib.getArchiveFileList(archiveFile.getAbsolutePath());

        if (rawDataArray == null) {
            showToast("Gagal membuka arsip (Format tidak didukung/Encrypted)");
            return;
        }

        isInArchiveMode = true;
        currentArchiveFile = archiveFile;
        currentVirtualPath = "";

        rawArchiveData.clear();
        Collections.addAll(rawArchiveData, rawDataArray);

        refreshVirtualView();
    }

    private void navigateUpVirtual() {
        if (currentVirtualPath.endsWith("/")) {
            String temp = currentVirtualPath.substring(0, currentVirtualPath.length() - 1);
            int lastSlash = temp.lastIndexOf('/');
            if (lastSlash != -1) {
                currentVirtualPath = temp.substring(0, lastSlash + 1);
            } else {
                currentVirtualPath = "";
            }
        } else {
            currentVirtualPath = "";
        }
        refreshVirtualView();
    }

    private void exitArchiveMode() {
        isInArchiveMode = false;
        rawArchiveData.clear();
        if (currentArchiveFile != null) {
            loadDirectory(currentArchiveFile.getParentFile());
        }
        currentArchiveFile = null;
    }


    private void refreshVirtualView() {
        List<FileModel> displayList = new ArrayList<>();
        Set<String> addedFolders = new HashSet<>();

        getBinding().tvPath.setText(currentArchiveFile.getName() + " > /" + currentVirtualPath);

        for (String entry : rawArchiveData) {
            String[] parts = entry.split("\\|");
            String fullPath = parts[0];
            long size = (parts.length > 1) ? Long.parseLong(parts[1]) : 0;

            if (fullPath.startsWith(currentVirtualPath)) {
                String remainingPath = fullPath.substring(currentVirtualPath.length());
                int slashIndex = remainingPath.indexOf('/');

                if (slashIndex != -1) {
                    String folderName = remainingPath.substring(0, slashIndex);
                    if (!addedFolders.contains(folderName)) {
                        FileModel folderModel = new FileModel(folderName, 0);
                        folderModel.isDirectory = true;
                        displayList.add(folderModel);
                        addedFolders.add(folderName);
                    }
                } else {
                    if (!remainingPath.isEmpty()) {
                        displayList.add(new FileModel(remainingPath, size));
                    }
                }
            }
        }

        Collections.sort(displayList, (o1, o2) -> {
            if (o1.isDirectory && !o2.isDirectory) return -1;
            if (!o1.isDirectory && o2.isDirectory) return 1;
            return o1.name.compareToIgnoreCase(o2.name);
        });

        adapter.updateList(displayList);
        updateEmptyState(displayList.isEmpty());
    }

    private boolean isSupportedArchive(String fileName) {
        String lower = fileName.toLowerCase();
        return lower.endsWith(".xp3") || lower.endsWith(".pfs") || lower.endsWith(".pfs.000") || lower.endsWith(".pfs.001") || lower.endsWith(".arc");
    }

    @Override
    public void onFileClick(FileModel file) {
        if (isInArchiveMode) {
            if (file.isDirectory) {
                currentVirtualPath += file.name + "/";
                refreshVirtualView();
            }
            else {
                String name = file.name.toLowerCase();

                if (name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".bmp")) {
                    showImagePreview(file, false);
                }
                else if (name.endsWith(".tlg")) {
                    showImagePreview(file, true);
                }

                else if (name.endsWith(".tjs") || name.endsWith(".ks") ||
                        name.endsWith(".ini") || name.endsWith(".lua") ||
                        name.endsWith(".txt") || name.endsWith(".scn")) {
                    showTextPreview(file);
                }

                else {
                    checkAndShowPreview(file);

                }
            }
        } else {
            if (file.isDirectory) loadDirectory(file.file);
            else if (file.isXp3 || isSupportedArchive(file.name)) enterArchiveMode(file.file);
            else showToast("File System: " + file.name);
        }
    }

    private void checkAndShowPreview(FileModel file) {
        new Thread(() -> {
            byte[] data = nativeLib.getFileBuffer(currentArchiveFile.getAbsolutePath(), currentVirtualPath + file.name);

            if (data != null && data.length > 32) {

                String signature = new String(data, 0, 12);
                if (signature.equals("CompressedBG")) {
                    requireActivity().runOnUiThread(() -> showBgiPreview(file));
                    return;
                }
            }

            requireActivity().runOnUiThread(() ->
                    showToast("File: " + file.name + "\n(Gunakan titik tiga untuk ekstrak)"));

        }).start();
    }

    private void showBgiPreview(FileModel file) {
        AlertDialog loadingDialog = new AlertDialog.Builder(requireContext())
                .setMessage("Decoding BGI...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        new Thread(() -> {
            Bitmap bitmap = null;
            try {

                int[] rawData = nativeLib.getBgiPreview(
                        currentArchiveFile.getAbsolutePath(),
                        currentVirtualPath + file.name
                );

                if (rawData != null && rawData.length > 2) {
                    int width = rawData[0];
                    int height = rawData[1];
                    bitmap = Bitmap.createBitmap(rawData, 2, width, width, height, Bitmap.Config.ARGB_8888);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }

            Bitmap finalBitmap = bitmap;
            requireActivity().runOnUiThread(() -> {
                loadingDialog.dismiss();
                if (finalBitmap != null) {
                    showPreviewDialog(file, finalBitmap, true);
                } else {
                    showToast("Gagal decode BGI Image.");
                }
            });
        }).start();
    }

    private void showTextPreview(FileModel file) {
        AlertDialog loadingDialog = new AlertDialog.Builder(requireContext())
                .setMessage("Reading text...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        String fullInternalPath = currentVirtualPath + file.name;

        new Thread(() -> {

            byte[] data = nativeLib.getFileBuffer(
                    currentArchiveFile.getAbsolutePath(),
                    fullInternalPath
            );

            requireActivity().runOnUiThread(() -> {
                loadingDialog.dismiss();
                if (data == null || data.length == 0) {
                    showToast("Gagal membaca file atau file kosong.");
                    return;
                }

                if (file.name.toLowerCase().endsWith(".tjs") && data.length >= 7) {
                    if (data[0] == 0x54 && data[1] == 0x4A && data[2] == 0x53 &&
                            data[3] == 0x32 && data[4] == 0x31 && data[5] == 0x30 && data[6] == 0x30) {

                        showToast("File tercompile (Binary TJS). Akan ada di update mendatang");
                        return;
                    }
                }

                String textContent = decodeBytesToString(data);
                showTextDialog(file, textContent);
            });
        }).start();
    }

    private String decodeBytesToString(byte[] data) {
        if (data.length < 2) return new String(data);

        if ((data[0] & 0xFF) == 0xFF && (data[1] & 0xFF) == 0xFE) {
            return new String(data, StandardCharsets.UTF_16LE);
        }

        if ((data[0] & 0xFF) == 0xFE && (data[1] & 0xFF) == 0xFF) {
            return new String(data, StandardCharsets.UTF_16BE);
        }

        if (data.length >= 3 && (data[0] & 0xFF) == 0xEF && (data[1] & 0xFF) == 0xBB && (data[2] & 0xFF) == 0xBF) {
            return new String(data, StandardCharsets.UTF_8);
        }

        try {

            return new String(data, Charset.forName("Shift_JIS"));
        } catch (Exception e) {

            return new String(data, StandardCharsets.UTF_8);
        }
    }

    private void showTextDialog(FileModel file, String content) {
        AlertDialog.Builder builder = new AlertDialog.Builder(requireContext());
        builder.setTitle(file.name);


        ScrollView scrollView = new ScrollView(requireContext());
        TextView textView = new TextView(requireContext());

        textView.setText(content);
        textView.setPadding(30, 30, 30, 30);
        textView.setTextSize(12);
        textView.setTextColor(Color.WHITE);

        textView.setTypeface(android.graphics.Typeface.MONOSPACE);

        textView.setTextIsSelectable(true);

        scrollView.addView(textView);
        builder.setView(scrollView);

        builder.setPositiveButton("Close", null);
        builder.setNeutralButton("Extract", (dialog, which) -> {
            List<String> singlePath = Collections.singletonList(currentVirtualPath + file.name);
            performExtraction(singlePath);
        });

        builder.show();
    }

    private int calculateInSampleSize(BitmapFactory.Options options, int reqWidth, int reqHeight) {
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > reqHeight || width > reqWidth) {
            final int halfHeight = height / 2;
            final int halfWidth = width / 2;
            while ((halfHeight / inSampleSize) >= reqHeight && (halfWidth / inSampleSize) >= reqWidth) {
                inSampleSize *= 2;
            }
        }
        return inSampleSize;
    }

    private void showImagePreview(FileModel file, boolean isTlg) {

        AlertDialog loadingDialog = new AlertDialog.Builder(requireContext())
                .setMessage("Loading preview...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        String fullInternalPath = currentVirtualPath + file.name;

        new Thread(() -> {
            Bitmap bitmap = null;
            try {
                if (isTlg) {
                    int[] rawData = nativeLib.getTlgPreview(
                            currentArchiveFile.getAbsolutePath(),
                            fullInternalPath
                    );
                    if (rawData != null && rawData.length > 2) {
                        int width = rawData[0];
                        int height = rawData[1];
                        bitmap = Bitmap.createBitmap(rawData, 2, width, width, height, Bitmap.Config.ARGB_8888);
                    }
                } else {
                    byte[] imageData = nativeLib.getFileBuffer(
                            currentArchiveFile.getAbsolutePath(),
                            fullInternalPath
                    );
                    if (imageData != null && imageData.length > 0) {
                        BitmapFactory.Options options = new BitmapFactory.Options();
                        options.inJustDecodeBounds = true;
                        BitmapFactory.decodeByteArray(imageData, 0, imageData.length, options);
                        options.inSampleSize = calculateInSampleSize(options, 800, 800);
                        options.inJustDecodeBounds = false;

                        bitmap = BitmapFactory.decodeByteArray(imageData, 0, imageData.length, options);
                    }
                }
            } catch (Exception | OutOfMemoryError e) {
                e.printStackTrace();
            }

            Bitmap finalBitmap = bitmap;
            requireActivity().runOnUiThread(() -> {
                loadingDialog.dismiss();
                if (finalBitmap != null) {
                    showPreviewDialog(file, finalBitmap, isTlg);
                } else {
                    showToast("Gagal memuat preview gambar.");
                }
            });
        }).start();
    }

    private void showPreviewDialog(FileModel file, Bitmap bitmap, boolean showConvertOption) {
        AlertDialog.Builder builder = new AlertDialog.Builder(requireContext());
        builder.setTitle(file.name);

        ImageView imageView = new ImageView(requireContext());
        imageView.setImageBitmap(bitmap);
        imageView.setAdjustViewBounds(true);
        imageView.setScaleType(ImageView.ScaleType.FIT_CENTER);
        imageView.setBackgroundColor(Color.DKGRAY);
        imageView.setPadding(10, 10, 10, 10);

        builder.setView(imageView);

        builder.setPositiveButton("Close", (dialog, which) -> {
            if (bitmap != null && !bitmap.isRecycled()) {
                bitmap.recycle();
            }
        });

        builder.setNeutralButton("Extract", (dialog, which) -> {
            List<String> singlePath = Collections.singletonList(currentVirtualPath + file.name);
            performExtraction(singlePath);
        });

        if (showConvertOption) {
            builder.setNegativeButton("Convert", (dialog, which) -> {
                saveBitmapAsPng(file, bitmap);
                if (bitmap != null && !bitmap.isRecycled()) bitmap.recycle();
            });
        }

        AlertDialog dialog = builder.create();
        dialog.show();
    }

    private void saveBitmapAsPng(FileModel file, Bitmap bitmap) {
        if (currentArchiveFile == null || bitmap == null) return;

        File parentDir = currentArchiveFile.getParentFile();
        String xp3Name = currentArchiveFile.getName().replaceFirst("[.][^.]+$", ""); // Buang ekstensi arsip
        File targetBaseDir = new File(parentDir, "Gardroid_Extracted/" + xp3Name);

        File targetDir = new File(targetBaseDir, currentVirtualPath);
        if (!targetDir.exists()) targetDir.mkdirs();

        String originalName = file.name;
        String pngName;
        int dotIndex = originalName.lastIndexOf('.');
        if (dotIndex > 0) {
            pngName = originalName.substring(0, dotIndex) + ".png";
        } else {
            pngName = originalName + ".png";
        }

        File destFile = new File(targetDir, pngName);

        try (FileOutputStream out = new FileOutputStream(destFile)) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
            showToast("Converted & Saved:\n" + destFile.getAbsolutePath());
        } catch (IOException e) {
            e.printStackTrace();
            showToast("Gagal menyimpan PNG: " + e.getMessage());
        }
    }
    @Override
    public void onMoreClick(FileModel file) {
        List<String> options = new ArrayList<>();
        String title = "Options: " + file.name;
        if (file.isDirectory && !isInArchiveMode) {
            options.add("Repack to Archive");
            options.add("Repack AST in this Folder");
            options.add("Repack KS in this Folder");
        }
        else if (isInArchiveMode && !file.isDirectory) {
            options.add("Extract File");
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
                        // --- EKSEKUSI AST PACKER ---
                        // Kirim path folder yang dipilih ke AstPackerManager
                        List<String> folderToProcess = Collections.singletonList(file.file.getAbsolutePath());
                        astPackerManager.processSelection(folderToProcess);
                    } else if (selected.equals("Repack KS in this Folder")) {
                        List<String> folderToProcess = Collections.singletonList(file.file.getAbsolutePath());
                        ksPackerManager.processSelection(folderToProcess);
                    }
                    else if (selected.equals("Extract File")) {
                        List<String> singlePath = Collections.singletonList(currentVirtualPath + file.name);
                        performExtraction(singlePath);
                    }

                })
                .show();
    }

    private void showRepackFormatDialog(FileModel folder) {
        String[] formats = {"Kirikiri (.xp3)", "Artemis (.pfs)"};

        new AlertDialog.Builder(requireContext())
                .setTitle("Select Archive Format")
                .setItems(formats, (dialog, which) -> {
                    if (which == 0) {
                        performRepack(Collections.singletonList(folder), "xp3");
                    } else {
                        performRepack(Collections.singletonList(folder), "pfs");
                    }
                })
                .show();
    }

    private void showExtractionOptions() {

        List<FileModel> selected = adapter.getSelectedFiles();
        boolean hasFolderSelected = false;
        for (FileModel f : selected) {
            if (f.isDirectory) {
                hasFolderSelected = true;
                break;
            }
        }

        List<String> optionsList = new ArrayList<>();
        if (isInArchiveMode) {
            optionsList.add("Extract Selected Files");
            optionsList.add("Extract ALL Files");
        } else if (hasFolderSelected) {
            optionsList.add("Repack Selected Folder");
        }

        if (optionsList.isEmpty()) {
            showToast("Tidak ada aksi tersedia.");
            return;
        }

        String[] options = optionsList.toArray(new String[0]);
        new AlertDialog.Builder(requireContext())
                .setTitle("Batch Actions")
                .setItems(options, (dialog, which) -> {
                    String choice = options[which];
                    if (choice.contains("Extract Selected")) {
                        handleExtractSelected(selected);
                    } else if (choice.contains("Extract ALL")) {
                        handleExtractAll();
                    } else if (choice.contains("Repack")) {
                        for(FileModel f : selected) {
                            if(f.isDirectory) {
                                showRepackFormatDialog(f);
                                break;
                            }
                        }
                    }
                })
                .show();
    }

    private void handleExtractSelected(List<FileModel> selected) {
        if (selected.isEmpty()) {
            showToast("Pilih file atau folder dulu!");
            return;
        }

        Set<String> uniquePathsToExtract = new HashSet<>();

        for (FileModel f : selected) {
            String fullItemPath = currentVirtualPath + f.name;
            if (!f.isDirectory) {
                uniquePathsToExtract.add(fullItemPath);
            } else {
                String folderPrefix = fullItemPath + "/";
                for (String entry : rawArchiveData) {
                    if (entry.startsWith(folderPrefix)) {
                        uniquePathsToExtract.add(entry.split("\\|")[0]);
                    }
                }
            }
        }
        List<String> finalPaths = new ArrayList<>(uniquePathsToExtract);

        if (astManager.containsAstFiles(finalPaths)) {
            astManager.configureAstExtraction(currentArchiveFile, finalPaths, (convertAst, language) -> {

                checkScriptsAndExtract(finalPaths, convertAst, language);
            });
        } else {

            checkScriptsAndExtract(finalPaths, false, null);
        }
    }

    private void handleExtractAll() {
        List<String> allPaths = new ArrayList<>();
        for (String entry : rawArchiveData) {
            allPaths.add(entry.split("\\|")[0]);
        }

        if (astManager.containsAstFiles(allPaths)) {

            astManager.configureAstExtraction(currentArchiveFile, allPaths, (convertAst, language) -> {
                checkScriptsAndExtract(allPaths, convertAst, language);
            });
        } else {

            checkScriptsAndExtract(allPaths, false, null);
        }
    }

    private void checkScriptsAndExtract(List<String> paths, boolean convertAst, String astLang) {
        // 1. Cek SCN
        if (scnManager.containsScnFiles(paths)) {
            scnManager.configureScnExtraction(paths, (convertScn) -> {
                checkKsAndExtract(paths, convertAst, astLang, convertScn);
            });
        } else {
            checkKsAndExtract(paths, convertAst, astLang, false);
        }
    }

    private void checkKsAndExtract(List<String> paths, boolean convertAst, String astLang, boolean convertScn) {
        // 2. Cek KS
        if (ksManager.containsKsFiles(paths)) {
            ksManager.configureKsExtraction(paths, (convertKs) -> {
                performExtraction(paths, convertAst, astLang, convertScn, convertKs);
            });
        } else {
            performExtraction(paths, convertAst, astLang, convertScn, false);
        }
    }

    private void performExtraction(List<String> internalPaths) {
        performExtraction(internalPaths, false, null, false, false);
    }


    private void performExtraction(List<String> internalPaths, boolean convertAst, String astLanguage, boolean convertScn, boolean convertKs) {
        if (internalPaths.isEmpty()) return;

        if (currentArchiveFile == null) {
            showToast("Error: Tidak ada arsip yang aktif.");
            return;
        }

        File parentDir = currentArchiveFile.getParentFile();
        String folderName = currentArchiveFile.getName().replaceFirst("[.][^.]+$", "");
        File targetDir = new File(parentDir, "Gardroid_Extracted/" + folderName);
        if (!targetDir.exists()) targetDir.mkdirs();
        AlertDialog.Builder builder = new AlertDialog.Builder(requireContext());
        builder.setTitle("Extracting...");
        builder.setCancelable(false);
        android.widget.LinearLayout layout = new android.widget.LinearLayout(requireContext());
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 10);
        final android.widget.ProgressBar progressBar = new android.widget.ProgressBar(requireContext(), null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(internalPaths.size());
        layout.addView(progressBar);

        final android.widget.TextView tvStatus = new android.widget.TextView(requireContext());
        tvStatus.setText("Initializing...");
        tvStatus.setPadding(0, 20, 0, 0);
        layout.addView(tvStatus);

        builder.setView(layout);
        AlertDialog progressDialog = builder.create();
        progressDialog.show();

        new Thread(() -> {
            long parserPointer = nativeLib.initParser(currentArchiveFile.getAbsolutePath());

            if (parserPointer == 0) {
                requireActivity().runOnUiThread(() -> {
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

                // [MODIFIKASI: Throttling UI Update (Maksimal update setiap 100ms)]
                long currentTime = System.currentTimeMillis();
                if (currentTime - lastUpdateTime > 100 || progress == internalPaths.size()) {
                    lastUpdateTime = currentTime;
                    requireActivity().runOnUiThread(() -> {
                        progressBar.setProgress(progress);
                        tvStatus.setText("Extracting (" + progress + " / " + internalPaths.size() + ")\n" + internalPath);
                    });
                }

                File destFile = new File(targetDir, internalPath);

                if (destFile.getParentFile() != null && !destFile.getParentFile().exists()) {
                    destFile.getParentFile().mkdirs();
                }

                boolean ok = nativeLib.extractFileFromPointer(parserPointer, internalPath, destFile.getAbsolutePath());

                if (ok) {
                    successCount++;
                    String destPath = destFile.getAbsolutePath();
                    String lowerPath = destPath.toLowerCase();
                    if (convertAst && lowerPath.endsWith(".ast") && astLanguage != null) {
                        astManager.processAstFile(destPath, astLanguage);
                    }
                    else if (convertScn && lowerPath.endsWith(".scn")) {
                        scnManager.processScnFile(destPath);
                    }
                    else if (convertKs && lowerPath.endsWith(".ks")) {
                        ksManager.processKsFile(destPath); // EKSEKUSI KS DI SINI
                    }
                }
            }


            nativeLib.closeParser(parserPointer);

            int finalSuccess = successCount;

            requireActivity().runOnUiThread(() -> {
                progressDialog.dismiss();
                showToast("Selesai! " + finalSuccess + " file tersimpan di:\n" + targetDir.getAbsolutePath());

                if (adapter != null) {
                    adapter.setSelectionMode(false);
                    // loadDirectory(currentVirtualPath);
                }
            });

        }).start();
    }

    private void performRepack(List<FileModel> selectedFiles, String format) {
        FileModel targetFolder = null;
        for (FileModel f : selectedFiles) { if (f.isDirectory) { targetFolder = f; break; }}
        if (targetFolder == null) return;

        File sourceDir = targetFolder.file;
        File parentDir = sourceDir.getParentFile();
        File outputFile = new File(parentDir, sourceDir.getName() + "." + format);
        AlertDialog.Builder builder = new AlertDialog.Builder(requireContext());
        builder.setTitle("Repacking " + sourceDir.getName());
        builder.setCancelable(false);

        android.widget.LinearLayout layout = new android.widget.LinearLayout(requireContext());
        layout.setOrientation(android.widget.LinearLayout.VERTICAL);
        layout.setPadding(50, 40, 50, 10);

        final android.widget.ProgressBar progressBar = new android.widget.ProgressBar(requireContext(), null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setIndeterminate(false);
        layout.addView(progressBar);

        final android.widget.TextView tvStatus = new android.widget.TextView(requireContext());
        tvStatus.setText("Scanning files...");
        layout.addView(tvStatus);

        builder.setView(layout);
        AlertDialog progressDialog = builder.create();
        progressDialog.show();

        new Thread(() -> {
            boolean success = false;

            NativeLib.RepackListener listener = (currentFile, current, total) -> {
                requireActivity().runOnUiThread(() -> {
                    if (progressBar.getMax() != total) progressBar.setMax(total);
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
            requireActivity().runOnUiThread(() -> {
                progressDialog.dismiss();
                if (finalSuccess) {
                    showToast("Repack " + format.toUpperCase() + " Berhasil!");
                    adapter.setSelectionMode(false);
                    loadDirectory(currentDirectory);
                } else {
                    showToast("Repack Gagal.");
                }
            });
        }).start();
    }
}