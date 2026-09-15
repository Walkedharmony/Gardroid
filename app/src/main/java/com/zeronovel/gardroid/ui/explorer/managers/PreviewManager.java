package com.zeronovel.gardroid.ui.explorer.managers;

import android.app.AlertDialog;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Environment;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.content.ContextCompat;

import com.zeronovel.gardroid.R;
import com.zeronovel.gardroid.data.model.FileModel;
import java.nio.charset.Charset;
import java.util.Collections;
import java.util.List;
import java.io.IOException;
import com.zeronovel.gardroid.ui.explorer.ExplorerFragment;
import com.zeronovel.gardroid.ui.explorer.ExplorerViewModel;
import com.zeronovel.gardroid.bridge.NativeLib;
import com.zeronovel.gardroid.utils.NativeAudioPlayer;
import com.zeronovel.gardroid.bridge.NativeAudioInfo;
import android.widget.SeekBar;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

public class PreviewManager {
    private final ExplorerFragment fragment;
    private final ExplorerViewModel viewModel;
    private final NativeLib nativeLib;

    public void showToast(String message) {
        if (fragment.getActivity() != null) {
            fragment.getActivity()
                    .runOnUiThread(() -> Toast.makeText(fragment.requireContext(), message, Toast.LENGTH_SHORT).show());
        }
    }

    public int calculateInSampleSize(BitmapFactory.Options options, int reqWidth, int reqHeight) {
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

    public PreviewManager(ExplorerFragment fragment, ExplorerViewModel viewModel, NativeLib nativeLib) {
        this.fragment = fragment;
        this.viewModel = viewModel;
        this.nativeLib = nativeLib;
    }

    public void checkAndShowPreview(FileModel file) {
        new Thread(() -> {
            byte[] data = nativeLib.getFileBuffer(viewModel.currentArchiveFile.getAbsolutePath(),
                    viewModel.currentVirtualPath + file.name);

            if (data != null && data.length > 32) {

                String signature = new String(data, 0, 12);
                if (signature.equals("CompressedBG")) {
                    fragment.requireActivity().runOnUiThread(() -> showModernPreview(file, viewModel.isInArchiveMode));
                    return;
                }
            }

            fragment.requireActivity()
                    .runOnUiThread(() -> showToast("File: " + file.name + "\n(Gunakan titik tiga untuk ekstrak)"));

        }).start();
    }

    public String decodeText(byte[] data) {
        if (data == null || data.length == 0)
            return "";
        if (data.length >= 2 && data[0] == (byte) 0xFF && data[1] == (byte) 0xFE) {
            return new String(data, 2, data.length - 2, java.nio.charset.StandardCharsets.UTF_16LE);
        }
        if (data.length >= 3 && data[0] == (byte) 0xEF && data[1] == (byte) 0xBB && data[2] == (byte) 0xBF) {
            return new String(data, 3, data.length - 3, java.nio.charset.StandardCharsets.UTF_8);
        }

        boolean isUtf8 = true;
        int i = 0;
        while (i < data.length) {
            int b = data[i] & 0xFF;
            if (b < 0x80) {
                i++;
            } else if (b >= 0xC2 && b <= 0xDF) {
                if (i + 1 < data.length && (data[i + 1] & 0xC0) == 0x80) {
                    i += 2;
                } else {
                    isUtf8 = false;
                    break;
                }
            } else if (b >= 0xE0 && b <= 0xEF) {
                if (i + 2 < data.length && (data[i + 1] & 0xC0) == 0x80 && (data[i + 2] & 0xC0) == 0x80) {
                    i += 3;
                } else {
                    isUtf8 = false;
                    break;
                }
            } else if (b >= 0xF0 && b <= 0xF4) {
                if (i + 3 < data.length && (data[i + 1] & 0xC0) == 0x80 && (data[i + 2] & 0xC0) == 0x80
                        && (data[i + 3] & 0xC0) == 0x80) {
                    i += 4;
                } else {
                    isUtf8 = false;
                    break;
                }
            } else {
                isUtf8 = false;
                break;
            }
        }

        if (isUtf8) {
            return new String(data, java.nio.charset.StandardCharsets.UTF_8);
        } else {
            try {
                return new String(data, "Shift_JIS");
            } catch (Exception e) {
                return new String(data, java.nio.charset.StandardCharsets.UTF_8);
            }
        }
    }

    public String detectRealFormat(String fileName, byte[] header) {
        if (header == null || header.length < 4) {
            return fileName.toUpperCase().substring(fileName.lastIndexOf('.') + 1);
        }

        // Check WebP
        if (header.length >= 12 && header[0] == 0x52 && header[1] == 0x49 && header[2] == 0x46 && header[3] == 0x46 &&
                header[8] == 0x57 && header[9] == 0x45 && header[10] == 0x42 && header[11] == 0x50) {
            return "WebP";
        }

        // Check TLG5
        if (header.length >= 11 && header[0] == 0x54 && header[1] == 0x4C && header[2] == 0x47 && header[3] == 0x35) {
            return "TLG5";
        }

        // Check TLG6
        if (header.length >= 11 && header[0] == 0x54 && header[1] == 0x4C && header[2] == 0x47 && header[3] == 0x36) {
            return "TLG6";
        }

        // Check TJS2100
        if (header.length >= 7 && header[0] == 0x54 && header[1] == 0x4A && header[2] == 0x53 &&
                header[3] == 0x32 && header[4] == 0x31 && header[5] == 0x30 && header[6] == 0x30) {
            return "TJS2100";
        }

        return fileName.toUpperCase().substring(fileName.lastIndexOf('.') + 1);
    }

    public void showModernPreview(FileModel file, boolean isArchiveMode) {
        AlertDialog loadingDialog = new AlertDialog.Builder(fragment.requireContext())
                .setMessage("Memuat preview...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        new Thread(() -> {
            String name = file.name.toLowerCase();
            boolean isImage = name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".bmp")
                    || name.endsWith(".tlg") || name.endsWith(".bgi");
            boolean isTlg = name.endsWith(".tlg");
            boolean isBgi = name.endsWith(".bgi");
            boolean isAudio = name.endsWith(".ogg") || name.endsWith(".opus");

            android.graphics.Bitmap bitmap = null;
            String textContent = null;
            byte[] rawBytes = null;
            byte[] header = new byte[16];
            String realFormat = "";

            try {
                if (isArchiveMode) {
                    String fullPath = viewModel.currentVirtualPath + file.name;
                    rawBytes = nativeLib.getFileBuffer(viewModel.currentArchiveFile.getAbsolutePath(), fullPath);
                    if (rawBytes != null && rawBytes.length >= 16) {
                        System.arraycopy(rawBytes, 0, header, 0, 16);
                    } else if (rawBytes != null && rawBytes.length > 0) {
                        header = rawBytes;
                    }
                    realFormat = detectRealFormat(file.name, header);

                    if (isTlg && realFormat.startsWith("TLG")) {
                        int[] rawData = nativeLib.getTlgPreview(viewModel.currentArchiveFile.getAbsolutePath(),
                                fullPath);
                        if (rawData != null && rawData.length > 2) {
                            bitmap = android.graphics.Bitmap.createBitmap(rawData, 2, rawData[0], rawData[0],
                                    rawData[1], android.graphics.Bitmap.Config.ARGB_8888);
                        }
                    } else if (isBgi) {
                        int[] rawData = nativeLib.getBgiPreview(viewModel.currentArchiveFile.getAbsolutePath(),
                                fullPath);
                        if (rawData != null && rawData.length > 2) {
                            bitmap = android.graphics.Bitmap.createBitmap(rawData, 2, rawData[0], rawData[0],
                                    rawData[1], android.graphics.Bitmap.Config.ARGB_8888);
                        }
                    } else if (isAudio) {
                        rawBytes = nativeLib.getFileBuffer(viewModel.currentArchiveFile.getAbsolutePath(), fullPath);
                        if (rawBytes != null && rawBytes.length > 0) {
                            try {
                                java.io.File tempAudio = new java.io.File(fragment.requireContext().getCacheDir(),
                                        file.name);
                                java.io.FileOutputStream fos = new java.io.FileOutputStream(tempAudio);
                                fos.write(rawBytes);
                                fos.close();
                                android.util.Log.d("PreviewManager", "Audio extracted successfully to: "
                                        + tempAudio.getAbsolutePath() + " (size: " + rawBytes.length + ")");
                            } catch (Exception e) {
                                android.util.Log.e("PreviewManager", "Failed to extract audio to cache", e);
                            }
                        } else {
                            android.util.Log.e("PreviewManager",
                                    "Failed to get audio buffer from archive (rawBytes is null or empty)");
                        }
                    } else {
                        rawBytes = nativeLib.getFileBuffer(viewModel.currentArchiveFile.getAbsolutePath(), fullPath);
                        if (rawBytes != null && rawBytes.length > 0) {
                            if (isImage) {
                                android.graphics.BitmapFactory.Options options = new android.graphics.BitmapFactory.Options();
                                options.inJustDecodeBounds = true;
                                android.graphics.BitmapFactory.decodeByteArray(rawBytes, 0, rawBytes.length, options);
                                options.inSampleSize = calculateInSampleSize(options, 800, 800);
                                options.inJustDecodeBounds = false;
                                bitmap = android.graphics.BitmapFactory.decodeByteArray(rawBytes, 0, rawBytes.length,
                                        options);
                            } else {
                                if (realFormat.equals("TJS2100")) {
                                    textContent = "File tercompile (Binary TJS). Akan ada di update mendatang";
                                } else {
                                    textContent = decodeText(rawBytes);
                                }
                            }
                        }
                    }
                } else {
                    // Mode diluar arsip
                    java.io.File f = file.file;
                    try (java.io.FileInputStream fis = new java.io.FileInputStream(f)) {
                        fis.read(header, 0, 16);
                    } catch (Exception e) {
                    }
                    realFormat = detectRealFormat(file.name, header);

                    if (realFormat.equals("TJS2100")) {
                        textContent = "File tercompile (Binary TJS). Akan ada di update mendatang";
                        isImage = false;
                    } else if (isTlg && realFormat.startsWith("TLG")) {
                        int[] rawData = nativeLib.getTlgPreviewOffline(file.file.getAbsolutePath());
                        if (rawData != null && rawData.length > 2) {
                            bitmap = android.graphics.Bitmap.createBitmap(rawData, 2, rawData[0], rawData[0],
                                    rawData[1], android.graphics.Bitmap.Config.ARGB_8888);
                        }
                    } else {

                        if (isImage) {
                            android.graphics.BitmapFactory.Options options = new android.graphics.BitmapFactory.Options();
                            options.inJustDecodeBounds = true;
                            android.graphics.BitmapFactory.decodeFile(f.getAbsolutePath(), options);
                            options.inSampleSize = calculateInSampleSize(options, 800, 800);
                            options.inJustDecodeBounds = false;
                            bitmap = android.graphics.BitmapFactory.decodeFile(f.getAbsolutePath(), options);
                        } else {

                            try (java.io.FileInputStream fis = new java.io.FileInputStream(f)) {
                                byte[] data = new byte[(int) f.length()];
                                fis.read(data);
                                textContent = decodeText(data);
                            }
                        }
                    }
                }
            } catch (Exception | OutOfMemoryError e) {
                e.printStackTrace();
            }

            android.graphics.Bitmap finalBitmap = bitmap;
            String finalString = textContent;
            String finalRealFormat = realFormat;

            fragment.requireActivity().runOnUiThread(() -> {
                loadingDialog.dismiss();
                if (finalBitmap == null && finalString == null && !isAudio) {
                    showToast("Gagal memuat file.");
                    return;
                }

                android.content.Context dialogContext = new android.view.ContextThemeWrapper(fragment.requireContext(),
                        com.google.android.material.R.style.ThemeOverlay_Material3_MaterialAlertDialog);
                android.view.View view = android.view.LayoutInflater.from(dialogContext)
                        .inflate(R.layout.dialog_preview, null);
                android.widget.TextView tvTag = view.findViewById(R.id.tvPreviewTag);
                android.widget.TextView tvFilename = view.findViewById(R.id.tvPreviewFilename);
                android.widget.ImageButton btnClose = view.findViewById(R.id.btnClosePreview);
                android.widget.ImageView ivImage = view.findViewById(R.id.ivImagePreview);
                android.widget.ScrollView svScript = view.findViewById(R.id.svScriptPreview);
                android.widget.TextView tvScript = view.findViewById(R.id.tvScriptPreview);
                android.widget.TextView tvResolution = view.findViewById(R.id.tvPreviewResolution);
                android.widget.TextView tvFormat = view.findViewById(R.id.tvPreviewFormat);
                android.widget.TextView tvSize = view.findViewById(R.id.tvPreviewSize);
                com.google.android.material.button.MaterialButton btnAction = view.findViewById(R.id.btnPreviewAction);

                android.view.View audioContainer = view.findViewById(R.id.audioPreviewContainer);
                com.google.android.material.floatingactionbutton.FloatingActionButton btnPlayPause = view
                        .findViewById(R.id.btnPlayPauseAudio);
                android.widget.TextView tvAudioCurrent = view.findViewById(R.id.tvAudioCurrentTime);
                android.widget.TextView tvAudioTotal = view.findViewById(R.id.tvAudioTotalTime);
                android.widget.SeekBar sbAudioProgress = view.findViewById(R.id.sbAudioProgress);
                android.widget.TextView tvAudioDuration = view.findViewById(R.id.tvAudioDuration);
                android.widget.TextView tvAudioFormatInfo = view.findViewById(R.id.tvAudioFormat);
                android.widget.TextView tvAudioCodec = view.findViewById(R.id.tvAudioCodec);

                tvFilename.setText(file.name);

                android.graphics.drawable.GradientDrawable tagBg = new android.graphics.drawable.GradientDrawable();
                tagBg.setCornerRadius(8f);

                tvFormat.setText("Format Asli: " + finalRealFormat);

                if (finalBitmap != null) {
                    tvTag.setText("IMAGE PREVIEW");
                    tvTag.setTextColor(android.graphics.Color.parseColor("#38BDF8"));
                    tagBg.setColor(android.graphics.Color.parseColor("#3338BDF8"));
                    tvTag.setBackground(tagBg);
                    ivImage.setVisibility(android.view.View.VISIBLE);
                    svScript.setVisibility(android.view.View.GONE);
                    if (audioContainer != null)
                        audioContainer.setVisibility(android.view.View.GONE);
                    ivImage.setImageBitmap(finalBitmap);
                    tvResolution.setText("Resolusi: " + finalBitmap.getWidth() + "x" + finalBitmap.getHeight());
                } else if (isAudio) {
                    tvTag.setText("AUDIO STREAM");
                    tvTag.setTextColor(android.graphics.Color.parseColor("#F59E0B"));
                    tagBg.setColor(android.graphics.Color.parseColor("#33F59E0B"));
                    tvTag.setBackground(tagBg);
                    ivImage.setVisibility(android.view.View.GONE);
                    svScript.setVisibility(android.view.View.GONE);
                    if (audioContainer != null)
                        audioContainer.setVisibility(android.view.View.VISIBLE);
                    tvResolution.setText("Audio: " + (name.endsWith(".ogg") ? "Vorbis" : "Opus"));
                } else {
                    tvTag.setText("SCRIPT PREVIEW");
                    tvTag.setTextColor(android.graphics.Color.parseColor("#34D399"));
                    tagBg.setColor(android.graphics.Color.parseColor("#3334D399"));
                    tvTag.setBackground(tagBg);
                    ivImage.setVisibility(android.view.View.GONE);
                    svScript.setVisibility(android.view.View.VISIBLE);
                    if (audioContainer != null)
                        audioContainer.setVisibility(android.view.View.GONE);
                    tvScript.setText(finalString);
                    tvResolution.setText("Lines: " + (finalString != null ? finalString.split("\\n").length : 0));
                }

                // Format & Size
                String ext = file.name.substring(file.name.lastIndexOf(".") + 1).toUpperCase();
                tvFormat.setText("Format Asli: " + ext);
                long bytes = isArchiveMode ? 0 : file.file.length();
                tvSize.setText("Ukuran: " + (bytes > 0 ? (bytes / 1024) + " KB" : "-"));

                NativeAudioPlayer audioPlayer = null;
                if (isAudio && audioContainer != null) {
                    audioPlayer = new NativeAudioPlayer(nativeLib);
                    String playPath = isArchiveMode
                            ? new java.io.File(fragment.requireContext().getCacheDir(), file.name).getAbsolutePath()
                            : file.file.getAbsolutePath();
                    android.util.Log.d("PreviewManager", "Opening audio path: " + playPath);
                    NativeAudioInfo info = audioPlayer.open(playPath);
                    if (info != null) {
                        android.util.Log.d("PreviewManager", "Audio opened successfully. Codec: " + info.codecName
                                + ", Duration: " + info.durationSeconds);
                        tvAudioDuration.setText(String.format(java.util.Locale.US, "Duration: %02d:%02d",
                                (int) (info.durationSeconds / 60), (int) (info.durationSeconds % 60)));
                        tvAudioFormatInfo.setText("Format: " + (info.format == 1 ? "OGG_VORBIS" : "OPUS"));
                        tvAudioCodec.setText("Codec: " + info.codecName);
                        tvAudioTotal.setText(String.format(java.util.Locale.US, "%02d:%02d",
                                (int) (info.durationSeconds / 60), (int) (info.durationSeconds % 60)));

                        NativeAudioPlayer finalAudioPlayer = audioPlayer;
                        audioPlayer.setPlaybackListener(new NativeAudioPlayer.PlaybackListener() {
                            @Override
                            public void onProgress(long currentSample, long totalSamples, double currentSeconds,
                                    double totalSeconds) {
                                tvAudioCurrent.setText(String.format(java.util.Locale.US, "%02d:%02d",
                                        (int) (currentSeconds / 60), (int) (currentSeconds % 60)));
                                sbAudioProgress.setProgress((int) ((currentSample * 100) / totalSamples));
                            }

                            @Override
                            public void onCompletion() {
                                btnPlayPause.setImageResource(android.R.drawable.ic_media_play);
                                sbAudioProgress.setProgress(100);
                            }

                            @Override
                            public void onError(String message) {
                                showToast("Audio Error: " + message);
                            }
                        });

                        btnPlayPause.setOnClickListener(v -> {
                            if (finalAudioPlayer.isPlaying()) {
                                finalAudioPlayer.pause();
                                btnPlayPause.setImageResource(android.R.drawable.ic_media_play);
                            } else {
                                finalAudioPlayer.play();
                                btnPlayPause.setImageResource(android.R.drawable.ic_media_pause);
                            }
                        });

                        sbAudioProgress.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                            @Override
                            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                            }

                            @Override
                            public void onStartTrackingTouch(SeekBar seekBar) {
                            }

                            @Override
                            public void onStopTrackingTouch(SeekBar seekBar) {
                                double targetSec = (seekBar.getProgress() / 100.0) * info.durationSeconds;
                                finalAudioPlayer.seekTo(targetSec);
                            }
                        });
                    } else {
                        android.util.Log.e("PreviewManager", "AudioPlayer.open returned null for path: " + playPath);
                    }
                }

                androidx.appcompat.app.AlertDialog previewDialog = new com.google.android.material.dialog.MaterialAlertDialogBuilder(
                        fragment.requireContext(),
                        com.google.android.material.R.style.ThemeOverlay_Material3_MaterialAlertDialog)
                        .setView(view)
                        .setCancelable(true)
                        .create();
                if (previewDialog.getWindow() != null) {
                    previewDialog.getWindow().setBackgroundDrawable(
                            new android.graphics.drawable.ColorDrawable(android.graphics.Color.TRANSPARENT));
                }

                NativeAudioPlayer finalAudioPlayerForDismiss = audioPlayer;
                btnClose.setOnClickListener(v -> {
                    if (finalAudioPlayerForDismiss != null)
                        finalAudioPlayerForDismiss.release();
                    previewDialog.dismiss();
                });

                previewDialog.setOnDismissListener(dialog -> {
                    if (finalAudioPlayerForDismiss != null)
                        finalAudioPlayerForDismiss.release();
                });

                btnAction.setOnClickListener(v -> {
                    if (isArchiveMode) {
                        previewDialog.dismiss();
                        java.util.List<String> singlePath = java.util.Collections
                                .singletonList(viewModel.currentVirtualPath + file.name);
                        fragment.performExtraction(singlePath);
                    } else if (name.endsWith(".tlg")) {
                        previewDialog.dismiss();
                        // Convert to RAW
                        convertTlgToPng(file.file);
                    } else {
                        showToast("File ini sudah di luar arsip.");
                    }
                });

                if (!isArchiveMode && !name.endsWith(".tlg")) {
                    btnAction.setVisibility(android.view.View.GONE);
                } else if (!isArchiveMode && name.endsWith(".tlg")) {
                    btnAction.setText("Convert to PNG");
                } else {
                    btnAction.setText("Extract File");
                }

                previewDialog.show();
            });
        }).start();
    }

    public void convertTlgToPng(java.io.File tlgFile) {
        AlertDialog loading = new AlertDialog.Builder(fragment.requireContext())
                .setMessage("Mengkonversi...")
                .setCancelable(false)
                .create();
        loading.show();

        new Thread(() -> {
            try {
                int[] rawData = nativeLib.getTlgPreviewOffline(tlgFile.getAbsolutePath());
                if (rawData != null && rawData.length > 2) {
                    android.graphics.Bitmap bmp = android.graphics.Bitmap.createBitmap(rawData, 2, rawData[0],
                            rawData[0], rawData[1], android.graphics.Bitmap.Config.ARGB_8888);
                    java.io.File targetFolder = tlgFile.getParentFile();
                    if (targetFolder == null)
                        targetFolder = new java.io.File(android.os.Environment.getExternalStorageDirectory(),
                                "Gardroid_Extracted");

                    java.io.File outFile = new java.io.File(targetFolder, tlgFile.getName().replace(".tlg", ".png"));
                    java.io.FileOutputStream out = new java.io.FileOutputStream(outFile);
                    bmp.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, out);
                    out.flush();
                    out.close();

                    fragment.requireActivity().runOnUiThread(() -> {
                        loading.dismiss();
                        showToast("Tersimpan di Gardroid_Extracted/" + outFile.getName());
                    });
                } else {
                    fragment.requireActivity().runOnUiThread(() -> {
                        loading.dismiss();
                        showToast("Gagal konversi TLG");
                    });
                }
            } catch (Exception e) {
                fragment.requireActivity().runOnUiThread(() -> {
                    loading.dismiss();
                    showToast("Error: " + e.getMessage());
                });
            }
        }).start();
    }

    public void showBgiPreview(FileModel file) {
        AlertDialog loadingDialog = new AlertDialog.Builder(fragment.requireContext())
                .setMessage("Decoding BGI...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        new Thread(() -> {
            Bitmap bitmap = null;
            try {

                int[] rawData = nativeLib.getBgiPreview(
                        viewModel.currentArchiveFile.getAbsolutePath(),
                        viewModel.currentVirtualPath + file.name);

                if (rawData != null && rawData.length > 2) {
                    int width = rawData[0];
                    int height = rawData[1];
                    bitmap = Bitmap.createBitmap(rawData, 2, width, width, height, Bitmap.Config.ARGB_8888);
                }
            } catch (Exception e) {
                e.printStackTrace();
            }

            Bitmap finalBitmap = bitmap;
            fragment.requireActivity().runOnUiThread(() -> {
                loadingDialog.dismiss();
                if (finalBitmap != null) {
                    showPreviewDialog(file, finalBitmap, true);
                } else {
                    showToast("Gagal decode BGI Image.");
                }
            });
        }).start();
    }

    public void showTextPreview(FileModel file) {
        AlertDialog loadingDialog = new AlertDialog.Builder(fragment.requireContext())
                .setMessage("Reading text...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        String fullInternalPath = viewModel.currentVirtualPath + file.name;

        new Thread(() -> {

            byte[] data = nativeLib.getFileBuffer(
                    viewModel.currentArchiveFile.getAbsolutePath(),
                    fullInternalPath);

            fragment.requireActivity().runOnUiThread(() -> {
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

    public String decodeBytesToString(byte[] data) {
        if (data.length < 2)
            return new String(data);

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

    public void showTextDialog(FileModel file, String content) {
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.requireContext());
        builder.setTitle(file.name);

        ScrollView scrollView = new ScrollView(fragment.requireContext());
        TextView textView = new TextView(fragment.requireContext());

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
            List<String> singlePath = Collections.singletonList(viewModel.currentVirtualPath + file.name);
            fragment.performExtraction(singlePath);
        });

        builder.show();
    }

    public void showImagePreview(FileModel file, boolean isTlg) {

        AlertDialog loadingDialog = new AlertDialog.Builder(fragment.requireContext())
                .setMessage("Loading preview...")
                .setCancelable(false)
                .create();
        loadingDialog.show();

        String fullInternalPath = viewModel.currentVirtualPath + file.name;

        new Thread(() -> {
            Bitmap bitmap = null;
            try {
                if (isTlg) {
                    int[] rawData = nativeLib.getTlgPreview(
                            viewModel.currentArchiveFile.getAbsolutePath(),
                            fullInternalPath);
                    if (rawData != null && rawData.length > 2) {
                        int width = rawData[0];
                        int height = rawData[1];
                        bitmap = Bitmap.createBitmap(rawData, 2, width, width, height, Bitmap.Config.ARGB_8888);
                    }
                } else {
                    byte[] imageData = nativeLib.getFileBuffer(
                            viewModel.currentArchiveFile.getAbsolutePath(),
                            fullInternalPath);
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
            fragment.requireActivity().runOnUiThread(() -> {
                loadingDialog.dismiss();
                if (finalBitmap != null) {
                    showPreviewDialog(file, finalBitmap, !isTlg);
                } else {
                    showToast("Gagal memuat preview gambar.");
                }
            });
        }).start();
    }

    public void showPreviewDialog(FileModel file, Bitmap bitmap, boolean showConvertOption) {
        AlertDialog.Builder builder = new AlertDialog.Builder(fragment.requireContext());
        builder.setTitle(file.name);

        ImageView imageView = new ImageView(fragment.requireContext());
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
            List<String> singlePath = Collections.singletonList(viewModel.currentVirtualPath + file.name);
            fragment.performExtraction(singlePath);
        });

        if (showConvertOption) {
            builder.setNegativeButton("Convert", (dialog, which) -> {
                saveBitmapAsPng(file, bitmap);
                if (bitmap != null && !bitmap.isRecycled())
                    bitmap.recycle();
            });
        }

        AlertDialog dialog = builder.create();
        dialog.show();
    }

    public void saveBitmapAsPng(FileModel file, Bitmap bitmap) {
        if (viewModel.currentArchiveFile == null || bitmap == null)
            return;

        File parentDir = viewModel.currentArchiveFile.getParentFile();
        String xp3Name = viewModel.currentArchiveFile.getName().replaceFirst("[.][^.]+$", "");
        File targetBaseDir = new File(parentDir, "Gardroid_Extracted/" + xp3Name);

        File targetDir = new File(targetBaseDir, viewModel.currentVirtualPath);
        if (!targetDir.exists())
            targetDir.mkdirs();

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

}
