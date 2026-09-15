package com.zeronovel.gardroid.data.repository;

import com.zeronovel.gardroid.data.model.FileModel;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class LocalFileRepository {
    public List<FileModel> getFilesInDirectory(File directory) {
        File[] files = directory.listFiles();
        List<FileModel> models = new ArrayList<>();
        if (files != null) {
            for (File file : files) {
                if (!file.isHidden()) models.add(new FileModel(file));
            }
        }
        
        // Android SAF/Scoped Storage workaround
        String path = directory.getAbsolutePath();
        if (path.equals("/storage/emulated") && (files == null || files.length == 0)) {
            models.add(new FileModel(new File("/storage/emulated/0")));
        } else if (path.equals("/storage") && (files == null || files.length == 0)) {
            models.add(new FileModel(new File("/storage/emulated")));
        }

        Collections.sort(models, (o1, o2) -> {
            if (o1.isDirectory && !o2.isDirectory) return -1;
            if (!o1.isDirectory && o2.isDirectory) return 1;
            return o1.name.compareToIgnoreCase(o2.name);
        });
        return models;
    }
}
