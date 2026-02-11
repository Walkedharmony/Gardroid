package com.zeronovel.gardroid.ui.base;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.viewbinding.ViewBinding;

public abstract class BaseFragment<VB extends ViewBinding> extends Fragment {

    private VB _binding;

    protected VB getBinding() {
        return _binding;
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        _binding = inflateBinding(inflater, container);
        return _binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        setupToolbar();
        setupViews();
        observeData();
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        _binding = null;
    }

    protected abstract VB inflateBinding(LayoutInflater inflater, ViewGroup container);

    protected abstract void setupViews();

    protected abstract void observeData();

    protected void showToast(String message) {
        if (getContext() != null) {
            Toast.makeText(getContext(), message, Toast.LENGTH_SHORT).show();
        }
    }

    protected void setupToolbar(Toolbar toolbar, String title, boolean showBackButton) {
        if (getActivity() instanceof AppCompatActivity) {
            AppCompatActivity activity = (AppCompatActivity) getActivity();
            activity.setSupportActionBar(toolbar);
            ActionBar actionBar = activity.getSupportActionBar();
            if (actionBar != null) {
                actionBar.setTitle(title);
                actionBar.setDisplayHomeAsUpEnabled(showBackButton);
                if (showBackButton) {
                    toolbar.setNavigationOnClickListener(v -> activity.onBackPressed());
                }
            }
        }
    }

    protected void setupToolbar() {
    }
}
