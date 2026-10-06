package com.nexdrum.aiora;

import android.app.NativeActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

public class AioraActivity extends NativeActivity {
    public static final int REQUEST_SAVE_JSON = 4101;
    public static final int REQUEST_LOAD_JSON = 4102;
    public static final int REQUEST_EXPORT_WAV = 4103;
    public static final int REQUEST_EXPORT_MIDI = 4104;

    private final Map<Integer, String> pendingSources = new HashMap<>();

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
    }

    public void createDocument(
            int requestCode,
            String suggestedName,
            String mimeType,
            String sourcePath) {

        runOnUiThread(() -> {
            try {
                pendingSources.put(requestCode, sourcePath == null ? "" : sourcePath);
                Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType(
                        mimeType == null || mimeType.isEmpty()
                                ? "application/octet-stream"
                                : mimeType);
                intent.putExtra(
                        Intent.EXTRA_TITLE,
                        suggestedName == null || suggestedName.isEmpty()
                                ? "Aiora"
                                : suggestedName);
                startActivityForResult(intent, requestCode);
            } catch (Exception e) {
                pendingSources.remove(requestCode);
                nativeDocumentResult(
                        requestCode, "", false,
                        e.getMessage() == null ? "Could not open Save As" : e.getMessage());
            }
        });
    }

    public void openDocument(int requestCode, String mimeType) {
        runOnUiThread(() -> {
            try {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                // Using */* keeps AIORA JSON visible even when a provider labels
                // .json files as text/plain or application/octet-stream.
                intent.setType("*/*");
                intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{
                        "application/json",
                        "text/json",
                        "text/plain",
                        "application/octet-stream"
                });
                startActivityForResult(intent, requestCode);
            } catch (Exception e) {
                nativeDocumentResult(
                        requestCode, "", false,
                        e.getMessage() == null ? "Could not open file picker" : e.getMessage());
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode != REQUEST_SAVE_JSON &&
            requestCode != REQUEST_LOAD_JSON &&
            requestCode != REQUEST_EXPORT_WAV &&
            requestCode != REQUEST_EXPORT_MIDI) {
            return;
        }

        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            pendingSources.remove(requestCode);
            nativeDocumentResult(requestCode, "", false, "Cancelled");
            return;
        }

        Uri uri = data.getData();
        try {
            if (requestCode == REQUEST_LOAD_JSON) {
                File dst = new File(getCacheDir(), "aiora_open_song.json");
                copyUriToFile(uri, dst);
                nativeDocumentResult(requestCode, dst.getAbsolutePath(), true, "");
                return;
            }

            String source = pendingSources.remove(requestCode);
            if (source == null || source.isEmpty()) {
                nativeDocumentResult(requestCode, "", false, "Export source file is missing");
                return;
            }

            copyFileToUri(new File(source), uri);
            nativeDocumentResult(requestCode, "", true, "");
        } catch (Exception e) {
            pendingSources.remove(requestCode);
            nativeDocumentResult(
                    requestCode, "", false,
                    e.getMessage() == null ? "Document operation failed" : e.getMessage());
        }
    }

    private void copyUriToFile(Uri uri, File dst) throws Exception {
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(dst, false)) {
            if (in == null) throw new Exception("Could not read selected file");
            copy(in, out);
        }
    }

    private void copyFileToUri(File src, Uri uri) throws Exception {
        if (!src.isFile()) throw new Exception("Prepared export file is missing");
        try (InputStream in = new FileInputStream(src);
             OutputStream out = getContentResolver().openOutputStream(uri, "wt")) {
            if (out == null) throw new Exception("Could not write selected destination");
            copy(in, out);
        }
    }

    private static void copy(InputStream in, OutputStream out) throws Exception {
        byte[] buffer = new byte[64 * 1024];
        int count;
        while ((count = in.read(buffer)) > 0) {
            out.write(buffer, 0, count);
        }
        out.flush();
    }

    private static native void nativeDocumentResult(
            int requestCode,
            String localPath,
            boolean success,
            String message);
}
