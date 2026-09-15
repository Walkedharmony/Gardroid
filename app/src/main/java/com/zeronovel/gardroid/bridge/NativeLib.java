package com.zeronovel.gardroid.bridge;

public class NativeLib {

    static {
        System.loadLibrary("gardroid");
    }

    /**
     * Fungsi Native untuk membaca file Archive (XP3/PFS/dll).
     * @param filePath Lokasi file di HP.
     * @return Array String berisi nama-nama file.
     */

    public native String[] getArchiveFileList(String filePath);

    public native byte[] getFileBuffer(String archivePath, String internalPath);

    public native int[] getTlgPreview(String archivePath, String internalPath);
    public native int[] getTlgPreviewOffline(String filePath);

    public native int[] getBgiPreview(String archivePath, String internalPath);
    public native boolean extractFile(String archivePath, String internalPath, String outputPath);

    public native long initParser(String archivePath);
    public native boolean extractFileFromPointer(long parserPointer, String internalPath, String outputPath);
    public native void closeParser(long parserPointer);

    public native String[] detectAstLanguages(String filePath);
    public native int extractAstText(String inputPath, String outputPath, String language);
    public native  int extractScnText(String inputPath, String outputPath);
    public native int extractKsText(String inputPath, String outputPath);
    public native int repackKsText(String ksPath, String txtPath, String outPath);
    public native int repackAst(String astPath, String txtPath, String outPath, String targetLang);
    public native boolean repackXp3(String sourceFolder, String outputFile, RepackListener listener);
    public native boolean repackPfs(String sourceFolder, String outputFile, RepackListener listener);
    public interface RepackListener {
        void onProgress(String currentFile, int current, int total);
    }

    // --- Audio Decoder JNI ---
    public native long nativeOpenAudio(String path);
    public native NativeAudioInfo nativeGetAudioInfo(long handle);
    public native int nativeDecodeAudio(long handle, short[] buffer, int maxSamples);
    public native boolean nativeSeekAudio(long handle, long sampleOffset);
    public native void nativeCloseAudio(long handle);
}