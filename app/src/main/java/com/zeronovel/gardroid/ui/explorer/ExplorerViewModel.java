package com.zeronovel.gardroid.ui.explorer;

import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

import com.zeronovel.gardroid.bridge.NativeLib;
import com.zeronovel.gardroid.data.model.FileModel;
import com.zeronovel.gardroid.data.repository.ArchiveRepository;
import com.zeronovel.gardroid.data.repository.LocalFileRepository;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class ExplorerViewModel extends ViewModel {
    private final LocalFileRepository localFileRepository = new LocalFileRepository();
    private final ArchiveRepository archiveRepository = new ArchiveRepository(new NativeLib());

    private final MutableLiveData<List<FileModel>> _fileList = new MutableLiveData<>(new ArrayList<>());
    public LiveData<List<FileModel>> fileList = _fileList;

    private final MutableLiveData<String> _currentPathText = new MutableLiveData<>("");
    public LiveData<String> currentPathText = _currentPathText;

    private final MutableLiveData<Boolean> _isEmpty = new MutableLiveData<>(true);
    public LiveData<Boolean> isEmpty = _isEmpty;

    private final MutableLiveData<Boolean> _isAstPackerMode = new MutableLiveData<>(false);
    public LiveData<Boolean> isAstPackerMode = _isAstPackerMode;

    // State Variables
    public File currentDirectory;
    public File currentArchiveFile = null;
    public boolean isInArchiveMode = false;
    public String currentVirtualPath = "";
    public final List<String> rawArchiveData = new ArrayList<>();

    public void toggleAstPackerMode() {
        boolean current = _isAstPackerMode.getValue() != null ? _isAstPackerMode.getValue() : false;
        _isAstPackerMode.setValue(!current);
    }

    public void loadDirectory(File directory) {
        if (directory == null) return;
        currentDirectory = directory;
        _currentPathText.setValue(directory.getAbsolutePath());

        List<FileModel> models = localFileRepository.getFilesInDirectory(directory);
        _fileList.setValue(models);
        _isEmpty.setValue(models.isEmpty());
    }

    public boolean enterArchiveMode(File archiveFile) {
        String[] rawDataArray = archiveRepository.getArchiveFileList(archiveFile.getAbsolutePath());
        if (rawDataArray == null) {
            return false;
        }

        isInArchiveMode = true;
        currentArchiveFile = archiveFile;
        currentVirtualPath = "";

        rawArchiveData.clear();
        Collections.addAll(rawArchiveData, rawDataArray);

        refreshVirtualView();
        return true;
    }

    public void navigateUpVirtual() {
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

    public void exitArchiveMode() {
        isInArchiveMode = false;
        rawArchiveData.clear();
        if (currentArchiveFile != null) {
            loadDirectory(currentArchiveFile.getParentFile());
        }
        currentArchiveFile = null;
    }

    public void refreshVirtualView() {
        if (currentArchiveFile != null) {
            String fullPath = currentArchiveFile.getAbsolutePath();
            if (currentVirtualPath != null && !currentVirtualPath.isEmpty()) {
                fullPath += "/" + currentVirtualPath;
            }
            _currentPathText.setValue(fullPath);
        }

        List<FileModel> displayList = archiveRepository.getVirtualFiles(rawArchiveData, currentVirtualPath);
        _fileList.setValue(displayList);
        _isEmpty.setValue(displayList.isEmpty());
    }

    public void navigateIntoVirtualFolder(String folderName) {
        currentVirtualPath += folderName + "/";
        refreshVirtualView();
    }
}
