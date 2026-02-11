package com.zeronovel.gardroid.data.model;

import java.io.File;

public class FileModel {
    public File file;
    public String name;
    public long size;
    public boolean isDirectory;
    public boolean isXp3;
    public boolean isPfs;

    public boolean isBgi;
    public boolean isSelected = false;
    public boolean isVirtual;

    public FileModel(File file) {
        this.file = file;
        this.name = file.getName();
        this.size = file.length();
        this.isDirectory = file.isDirectory();

        String lowerName = this.name.toLowerCase();

        this.isXp3 = lowerName.endsWith(".xp3");

        this.isPfs = lowerName.endsWith(".pfs") || lowerName.matches(".*\\.pfs\\.\\d{3}$");

        this.isBgi = lowerName.endsWith(".arc");

        this.isVirtual = false;
    }

    public FileModel(String name, long size) {
        this.file = null;
        this.name = name;
        this.size = size;
        this.isDirectory = false;
        this.isXp3 = false;
        this.isPfs = false;
        this.isVirtual = true;
    }
}