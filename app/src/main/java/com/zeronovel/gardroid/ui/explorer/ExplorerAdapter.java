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
            for (FileModel f : fileList) f.isSelected = false;
        }
        notifyDataSetChanged();
    }

    public List<FileModel> getSelectedFiles() {
        List<FileModel> selected = new ArrayList<>();
        for (FileModel f : fileList) {
            if (f.isSelected) selected.add(f);
        }
        return selected;
    }

    public void selectAll() {
        for (FileModel f : fileList) f.isSelected = true;
        notifyDataSetChanged();
    }

    public void deselectAll() {
        for (FileModel f : fileList) f.isSelected = false;
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public FileViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        ItemFileBinding binding = ItemFileBinding.inflate(
                LayoutInflater.from(parent.getContext()), parent, false
        );
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

        public void bind(FileModel item) {
            binding.tvName.setText(item.name);
            int iconRes;
            int iconColor;

            if (item.isDirectory) {
                iconRes = R.drawable.ic_folder_open;
                iconColor = 0xFFFFC107;
                binding.tvDetails.setText("Folder");
            }
            else if (item.isXp3 || item.isPfs || item.isBgi) {
                iconRes = R.drawable.xp3_file;
                iconColor = 0xFF9C27B0;
                binding.tvDetails.setText("Garbro Archive");
            }
            else {

                String name = item.name.toLowerCase();
                long sizeKb = item.size / 1024;
                binding.tvDetails.setText(sizeKb + " KB");

                if (name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".bmp")) {
                    iconRes = R.drawable.ic_file_image;
                    iconColor = 0xFF03A9F4;
                } else if (name.endsWith(".ogg") || name.endsWith(".wav") || name.endsWith(".mp3")) {
                    iconRes = R.drawable.ic_file_audio;
                    iconColor = 0xFFE91E63;
                } else if (name.endsWith(".tjs") || name.endsWith(".ks") ||
                        name.endsWith(".txt") || name.endsWith(".ini") ||
                        name.endsWith(".lua")) {
                    iconRes = R.drawable.ic_file_code;
                    iconColor = 0xFF4CAF50;

                } else {
                    iconRes = android.R.drawable.ic_menu_sort_by_size;
                    iconColor = 0xFF757575;
                }
            }

            binding.ivIcon.setImageResource(iconRes);
            binding.ivIcon.setColorFilter(iconColor);

            binding.getRoot().setActivated(item.isSelected);


            if (item.isSelected) {
                binding.getRoot().setBackgroundColor(0x33BB86FC);
                binding.cbSelect.setVisibility(View.VISIBLE);
                binding.btnMore.setVisibility(View.GONE);
                binding.cbSelect.setOnCheckedChangeListener(null);
                binding.cbSelect.setChecked(true);
            } else {
                binding.getRoot().setBackgroundResource(0);
                binding.cbSelect.setVisibility(View.GONE);
                binding.btnMore.setVisibility(View.VISIBLE);
            }

            binding.getRoot().setOnClickListener(v -> {
                if (isSelectionMode) {

                    item.isSelected = !item.isSelected;
                    notifyItemChanged(getAdapterPosition());
                } else {

                    listener.onFileClick(item);
                }
            });

            binding.getRoot().setOnLongClickListener(v -> {
                if (!item.isVirtual) {
                    return false;
                }
                if (!isSelectionMode) {
                    setSelectionMode(true);
                    item.isSelected = true;
                    notifyDataSetChanged();
                    return true;
                }
                return false;
            });

            binding.cbSelect.setOnClickListener(v -> {
                if (isSelectionMode) {
                    item.isSelected = binding.cbSelect.isChecked();

                }
            });

            binding.btnMore.setOnClickListener(v -> listener.onMoreClick(item));
        }
    }
}