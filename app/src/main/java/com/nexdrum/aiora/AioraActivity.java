package com.nexdrum.aiora;

import android.app.NativeActivity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.app.AlertDialog;
import android.text.InputFilter;
import android.text.InputType;
import android.view.WindowManager;
import android.widget.EditText;

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
    public static final int REQUEST_EXPORT_PATCH = 4105;
    public static final int REQUEST_IMPORT_PATCH = 4106;
    public static final int REQUEST_RENAME_TRACK = 4201;
    public static final int REQUEST_RENAME_PAD = 4202;
    public static final int REQUEST_OPERATOR_RATIO = 4203;
    public static final int REQUEST_NEW_PROJECT = 4204;

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
                writeResult(requestCode, false, "Could not open Save As");
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
                writeResult(requestCode, false, "Could not open file picker");
            }
        });
    }

    public void showNameEditor(
            int requestCode,
            int targetIndex,
            String title,
            String currentName) {

        runOnUiThread(() -> {
            try {
                EditText input = new EditText(this);
                input.setSingleLine(true);
                input.setInputType(
                        InputType.TYPE_CLASS_TEXT |
                        InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
                input.setFilters(new InputFilter[]{new InputFilter.LengthFilter(24)});
                input.setText(currentName == null ? "" : currentName);
                input.setSelectAllOnFocus(true);
                input.setPadding(36, 18, 36, 18);

                AlertDialog dialog = new AlertDialog.Builder(this)
                        .setTitle(title == null ? "Rename" : title)
                        .setView(input)
                        .setPositiveButton("Save", (d, which) ->
                                writeRenameResult(
                                        requestCode,
                                        targetIndex,
                                        input.getText().toString()))
                        .setNegativeButton("Cancel", (d, which) ->
                                writeRenameResult(
                                        requestCode,
                                        targetIndex,
                                        null))
                        .create();

                dialog.setOnCancelListener(d ->
                        writeRenameResult(requestCode, targetIndex, null));
                dialog.setOnShowListener(d -> {
                    input.requestFocus();
                    if (dialog.getWindow() != null) {
                        dialog.getWindow().setSoftInputMode(
                                WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                    }
                });
                dialog.show();
            } catch (Exception e) {
                writeRenameResult(requestCode, targetIndex, null);
            }
        });
    }

    public void showConfirmation(
            int requestCode,
            String title,
            String message) {

        runOnUiThread(() -> {
            try {
                AlertDialog dialog = new AlertDialog.Builder(this)
                        .setTitle(title == null ? "Confirm" : title)
                        .setMessage(message == null ? "Are you sure?" : message)
                        .setPositiveButton("Yes", (d, which) ->
                                writeResult(requestCode, true, ""))
                        .setNegativeButton("No", (d, which) ->
                                writeResult(requestCode, false, "Cancelled"))
                        .create();

                dialog.setOnCancelListener(d ->
                        writeResult(requestCode, false, "Cancelled"));
                dialog.show();
            } catch (Exception e) {
                writeResult(requestCode, false, "Cancelled");
            }
        });
    }

    public void showNumberEditor(
            int requestCode,
            int targetIndex,
            String title,
            String currentValue) {

        runOnUiThread(() -> {
            try {
                EditText input = new EditText(this);
                input.setSingleLine(true);
                input.setInputType(
                        InputType.TYPE_CLASS_NUMBER |
                        InputType.TYPE_NUMBER_FLAG_DECIMAL);
                input.setFilters(new InputFilter[]{new InputFilter.LengthFilter(12)});
                input.setText(currentValue == null ? "" : currentValue);
                input.setSelectAllOnFocus(true);
                input.setPadding(36, 18, 36, 18);

                AlertDialog dialog = new AlertDialog.Builder(this)
                        .setTitle(title == null ? "Value" : title)
                        .setView(input)
                        .setPositiveButton("Set", (d, which) ->
                                writeRenameResult(
                                        requestCode,
                                        targetIndex,
                                        input.getText().toString()))
                        .setNegativeButton("Cancel", (d, which) ->
                                writeRenameResult(
                                        requestCode,
                                        targetIndex,
                                        null))
                        .create();

                dialog.setOnCancelListener(d ->
                        writeRenameResult(requestCode, targetIndex, null));
                dialog.setOnShowListener(d -> {
                    input.requestFocus();
                    if (dialog.getWindow() != null) {
                        dialog.getWindow().setSoftInputMode(
                                WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
                    }
                });
                dialog.show();
            } catch (Exception e) {
                writeRenameResult(requestCode, targetIndex, null);
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (requestCode != REQUEST_SAVE_JSON &&
            requestCode != REQUEST_LOAD_JSON &&
            requestCode != REQUEST_EXPORT_WAV &&
            requestCode != REQUEST_EXPORT_MIDI &&
            requestCode != REQUEST_EXPORT_PATCH &&
            requestCode != REQUEST_IMPORT_PATCH) {
            return;
        }

        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            pendingSources.remove(requestCode);
            writeResult(requestCode, false, "Cancelled");
            return;
        }

        Uri uri = data.getData();
        try {
            if (requestCode == REQUEST_LOAD_JSON) {
                File dst = new File(getFilesDir(), "aiora_open_song.json");
                copyUriToFile(uri, dst);
                writeResult(requestCode, true, "");
                return;
            }
            if (requestCode == REQUEST_IMPORT_PATCH) {
                File dst = new File(getFilesDir(), "aiora_open_patch.patch.json");
                copyUriToFile(uri, dst);
                writeResult(requestCode, true, "");
                return;
            }

            String source = pendingSources.remove(requestCode);
            if (source == null || source.isEmpty()) {
                writeResult(requestCode, false, "Export source file is missing");
                return;
            }

            copyFileToUri(new File(source), uri);
            writeResult(requestCode, true, "");
        } catch (Exception e) {
            pendingSources.remove(requestCode);
            writeResult(
                    requestCode, false,
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

    private void writeRenameResult(
            int requestCode,
            int targetIndex,
            String name) {
        File result = new File(getFilesDir(), "aiora_document_result.txt");
        File temp = new File(getFilesDir(), "aiora_document_result.tmp");
        boolean success = name != null;
        String safeName = name == null
                ? ""
                : name.replace('\n', ' ').replace('\r', ' ').trim();
        String payload =
                requestCode + "\n" +
                (success ? "1" : "0") + "\n" +
                targetIndex + "\n" +
                safeName;
        try (FileOutputStream out = new FileOutputStream(temp, false)) {
            out.write(payload.getBytes(java.nio.charset.StandardCharsets.UTF_8));
            out.flush();
        } catch (Exception ignored) {
            return;
        }
        if (!temp.renameTo(result)) {
            try (FileInputStream in = new FileInputStream(temp);
                 FileOutputStream out = new FileOutputStream(result, false)) {
                copy(in, out);
            } catch (Exception ignored) {
                // Rename failure must never terminate the NativeActivity.
            }
            temp.delete();
        }
    }

    private void writeResult(int requestCode, boolean success, String message) {
        File result = new File(getFilesDir(), "aiora_document_result.txt");
        File temp = new File(getFilesDir(), "aiora_document_result.tmp");
        String safeMessage = message == null ? "" : message.replace('\n', ' ').replace('\r', ' ');
        String payload = requestCode + "\n" + (success ? "1" : "0") + "\n" + safeMessage;
        try (FileOutputStream out = new FileOutputStream(temp, false)) {
            out.write(payload.getBytes(java.nio.charset.StandardCharsets.UTF_8));
            out.flush();
        } catch (Exception ignored) {
            return;
        }
        if (!temp.renameTo(result)) {
            try (FileInputStream in = new FileInputStream(temp);
                 FileOutputStream out = new FileOutputStream(result, false)) {
                copy(in, out);
            } catch (Exception ignored) {
                // A failed status marker must never crash the NativeActivity.
            }
            temp.delete();
        }
    }
}
