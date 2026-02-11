package com.zeronovel.gardroid;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import com.zeronovel.gardroid.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("gardroid");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

    }

    public native String stringFromJNI();
}