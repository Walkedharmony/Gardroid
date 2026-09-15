package com.zeronovel.gardroid.ui.explorer;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.zeronovel.gardroid.R;
import com.zeronovel.gardroid.databinding.ItemFileBinding;
import com.zeronovel.gardroid.data.model.FileModel;
import java.util.ArrayList;
import java.util.List;

public class ExplorerAdapter extends RecyclerView.Adapter<ExplorerAdapter.FileViewHolder> {

    private List<FileModel> fileList = new ArrayList<>();
    private final OnFileClickListener listener;
    public boolean isSelectionMode = false;

    public interface OnFileClickListener {
        void onFileClick(FileModel file);

        void onMoreClick(FileModel file);

        void onSelectionChanged(int count);
    }

    public ExplorerAdapter(OnFileClickListener listener) {
        this.listener = listener;
    }

    public void updateList(List<FileModel> newFiles) {
        this.fileList = newFiles;
        notifyDataSetChanged();
    }

    public void setSelectionMode(boolean active) {
        isSelectionMode = active;

        if (!active) {
            for (FileModel f : fileList)
                f.isSelected = false;
        }
        notifyDataSetChanged();
        listener.onSelectionChanged(getSelectedFiles().size());
    }

    public List<FileModel> getSelectedFiles() {
        List<FileModel> selected = new ArrayList<>();
        for (FileModel f : fileList) {
            if (f.isSelected)
                selected.add(f);
        }
        return selected;
    }

    public void selectAll() {
        for (FileModel f : fileList)
            f.isSelected = true;
        notifyDataSetChanged();
        listener.onSelectionChanged(getSelectedFiles().size());
    }

    public void deselectAll() {
        for (FileModel f : fileList)
            f.isSelected = false;
        notifyDataSetChanged();
        listener.onSelectionChanged(getSelectedFiles().size());
    }

    @NonNull
    @Override
    public FileViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        ItemFileBinding binding = ItemFileBinding.inflate(
                LayoutInflater.from(parent.getContext()), parent, false);
        return new FileViewHolder(binding);
    }

    @Override
    public void onBindViewHolder(@NonNull FileViewHolder holder, int position) {
        holder.bind(fileList.get(position));
    }

    @Override
    public int getItemCount() {
        return fileList.size();
    }

    class FileViewHolder extends RecyclerView.ViewHolder {
        private final ItemFileBinding binding;

        public FileViewHolder(ItemFileBinding binding) {
            super(binding.getRoot());
            this.binding = binding;
        }

        private int getThemeColor(int attrResId) {
            android.util.TypedValue typedValue = new android.util.TypedValue();
            binding.getRoot().getContext().getTheme().resolveAttribute(attrResId, typedValue, true);
            return typedValue.data;
        }

        public void bind(FileModel item) {
            binding.tvName.setText(item.name);
            int iconRes;
            int colorBgAttr;
            int colorTextAttr;

            if (item.isDirectory) {
                iconRes = R.drawable.ic_folder;
                colorBgAttr = R.attr.colorFolderBg;
                colorTextAttr = R.attr.colorFolderText;
                binding.tvDetails.setText("Folder");
            } else if (item.isXp3 || item.isPfs || item.isBgi) {
                iconRes = R.drawable.ic_archive;
                colorBgAttr = R.attr.colorArchiveBg;
                colorTextAttr = R.attr.colorArchiveText;
                binding.tvDetails.setText("Archive");
            } else {
                String name = item.name.toLowerCase();
                long sizeKb = item.size / 1024;
                binding.tvDetails.setText(sizeKb + " KB");

                if (name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".bmp") || name.endsWith(".tlg")) {
                    iconRes = R.drawable.ic_image;
                    colorBgAttr = R.attr.colorImageBg;
                    colorTextAttr = R.attr.colorImageText;
                } else if (name.endsWith(".ogg") || name.endsWith(".wav") || name.endsWith(".mp3")) {
                    iconRes = R.drawable.ic_text;
                    colorBgAttr = R.attr.colorAudioBg;
                    colorTextAttr = R.attr.colorAudioText;
                } else if (name.endsWith(".tjs") || name.endsWith(".ks") ||
                        name.endsWith(".txt") || name.endsWith(".ini") ||
                        name.endsWith(".lua") || name.endsWith(".scn") || name.endsWith(".ast")) {
                    iconRes = R.drawable.ic_text;
                    colorBgAttr = R.attr.colorScriptBg;
                    colorTextAttr = R.attr.colorScriptText;
                } else {
                    iconRes = R.drawable.ic_text;
                    colorBgAttr = R.attr.colorDefaultBg;
                    colorTextAttr = R.attr.colorDefaultText;
                }
            }

            binding.ivIcon.setImageResource(iconRes);
            binding.ivIconContainer.setCardBackgroundColor(getThemeColor(colorBgAttr));
            binding.ivIcon.setColorFilter(getThemeColor(colorTextAttr));

            binding.getRoot().setActivated(item.isSelected);

            if (item.isSelected) {
                binding.cbSelect.setVisibility(View.VISIBLE);
                binding.btnMore.setVisibility(View.GONE);
                binding.cbSelect.setOnCheckedChangeListener(null);
                binding.cbSelect.setChecked(true);
            } else {
                binding.cbSelect.setVisibility(View.GONE);
                binding.btnMore.setVisibility(View.VISIBLE);
            }

            binding.getRoot().setOnClickListener(v -> {
                if (isSelectionMode) {

                    item.isSelected = !item.isSelected;
                    notifyItemChanged(getAdapterPosition());
                    listener.onSelectionChanged(getSelectedFiles().size());
                } else {

                    listener.onFileClick(item);
                }
            });

            binding.getRoot().setOnLongClickListener(v -> {
                // if (!item.isVirtual) {
                // return false;
                // }
                if (!isSelectionMode) {
                    setSelectionMode(true);
                    item.isSelected = true;
                    notifyDataSetChanged();
                    listener.onSelectionChanged(getSelectedFiles().size());
                    return true;
                }
                return false;
            });

            binding.cbSelect.setOnClickListener(v -> {
                if (isSelectionMode) {
                    item.isSelected = binding.cbSelect.isChecked();
                    listener.onSelectionChanged(getSelectedFiles().size());
                }
            });

            binding.btnMore.setOnClickListener(v -> listener.onMoreClick(item));
        }
    }
}