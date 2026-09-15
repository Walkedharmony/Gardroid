package com.zeronovel.gardroid.data.repository;

import com.zeronovel.gardroid.bridge.NativeLib;
import com.zeronovel.gardroid.data.model.FileModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class ArchiveRepository {
    private final NativeLib nativeLib;

    public ArchiveRepository(NativeLib nativeLib) {
        this.nativeLib = nativeLib;
    }

    public String[] getArchiveFileList(String archivePath) {
        return nativeLib.getArchiveFileList(archivePath);
    }
    
    public List<FileModel> getVirtualFiles(List<String> rawArchiveData, String currentVirtualPath) {
        List<FileModel> displayList = new ArrayList<>();
        Set<String> addedFolders = new HashSet<>();

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
        
        return displayList;
    }
}
