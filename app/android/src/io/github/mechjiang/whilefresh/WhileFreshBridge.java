package io.github.mechjiang.whilefresh;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.graphics.BitmapFactory;
import android.media.ExifInterface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.util.Log;

import androidx.core.content.FileProvider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.UUID;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;

// Native import / export for the C++ PlatformServices (app/src/platform/android/). Every result is
// a JSON message handed to nativeOnMessage; see app/src/platform/bridge-event.cpp for the format.
// Messages produced before Qt is ready (cold start from a share) are queued until setNativeReady.
public final class WhileFreshBridge
{
    private static final String TAG = "WhileFresh";
    static final int REQUEST_PICK_IMAGES = 0x5701;
    static final int REQUEST_CAPTURE_RECEIPT = 0x5702;
    private static final int MAX_IMAGES_PER_IMPORT = 30;
    private static final long MAX_IMAGE_BYTES = 50L * 1024 * 1024;
    private static final long MAX_IMPORT_BYTES = 200L * 1024 * 1024;
    private static final int MAX_TEXT_CHARS = 20000;
    private static final long READ_TIMEOUT_SECONDS = 60;
    private static final String STATE_CAPTURE_PATH = "whilefresh.capturePath";
    private static final String IMPORTS_DIR = "imports";
    private static final String CAPTURES_DIR = "captures"; // the only directory exposed by FileProvider

    private static native void nativeOnMessage(String json);

    private static final Object lock = new Object();
    private static boolean nativeReady;
    private static final ArrayList<String> pending = new ArrayList<>();
    // One thread keeps results in request order; copying large images must stay off the UI thread.
    private static final ExecutorService worker = Executors.newSingleThreadExecutor();
    // Provider reads run here so a provider that never answers can be abandoned after a timeout.
    private static final ExecutorService readers = Executors.newCachedThreadPool();
    // A gallery or camera request is open from launch until its terminal message is posted.
    private static final AtomicBoolean requestInFlight = new AtomicBoolean(false);
    private static WeakReference<Activity> activityRef = new WeakReference<>(null);
    private static String capturePath; // UI thread only

    private interface ActivityTask
    {
        void run(Activity activity);
    }

    private static final class ImportException extends Exception
    {
        ImportException(String message)
        {
            super(message);
        }
    }

    private WhileFreshBridge()
    {
    }

    // ---- Called from C++ ----

    public static void setNativeReady(boolean ready)
    {
        synchronized (lock) {
            nativeReady = ready;
            if (!ready)
                return;
            for (String message : pending)
                nativeOnMessage(message);
            pending.clear();
        }
    }

    public static void pickImages()
    {
        if (!beginRequest("gallery"))
            return;
        runOnUi("gallery", new ActivityTask() {
            @Override
            public void run(Activity activity)
            {
                startPicker(activity);
            }
        });
    }

    public static void captureReceipt()
    {
        if (!beginRequest("camera"))
            return;
        runOnUi("camera", new ActivityTask() {
            @Override
            public void run(Activity activity)
            {
                startCamera(activity);
            }
        });
    }

    public static void shareText(final String text, final String title)
    {
        runOnUi(null, new ActivityTask() {
            @Override
            public void run(Activity activity)
            {
                Intent send = new Intent(Intent.ACTION_SEND);
                send.setType("text/plain");
                send.putExtra(Intent.EXTRA_TEXT, text);
                Intent chooser = Intent.createChooser(send, title);
                // We accept text shares ourselves; offering 趁鲜 as a target for its own list is noise.
                chooser.putExtra(Intent.EXTRA_EXCLUDE_COMPONENTS,
                                 new ComponentName[] { new ComponentName(activity, WhileFreshActivity.class) });
                try {
                    activity.startActivity(chooser);
                } catch (RuntimeException e) {
                    Log.w(TAG, "could not open the share sheet: " + e.getClass().getSimpleName());
                }
            }
        });
    }

    // ---- Called from WhileFreshActivity (UI thread) ----

    static void onActivityCreated(Activity activity, Bundle savedState)
    {
        activityRef = new WeakReference<>(activity);
        // The process may have been killed while the camera was open.
        if (savedState != null && capturePath == null)
            capturePath = savedState.getString(STATE_CAPTURE_PATH);
    }

    static void onSaveInstanceState(Bundle outState)
    {
        if (capturePath != null)
            outState.putString(STATE_CAPTURE_PATH, capturePath);
    }

    static void onActivityDestroyed(Activity activity)
    {
        if (activityRef.get() == activity)
            activityRef.clear();
    }

    static void handleIntent(Activity activity, Intent intent)
    {
        if (intent == null || (intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0)
            return;
        String action = intent.getAction();
        if (!Intent.ACTION_SEND.equals(action) && !Intent.ACTION_SEND_MULTIPLE.equals(action))
            return;

        final Context context = activity.getApplicationContext();
        final String type = intent.getType();
        final ArrayList<Uri> uris = streamUris(intent);
        String text = sharedText(intent);
        boolean textFirst = type != null && type.startsWith("text/");
        if (!uris.isEmpty() && !(textFirst && text != null)) {
            worker.execute(new Runnable() {
                @Override
                public void run()
                {
                    importUris(context, "share", uris, type, false);
                }
            });
            return;
        }
        if (text != null) {
            if (text.length() > MAX_TEXT_CHARS) {
                postFailed("share", "文字过长", false);
                return;
            }
            JSONObject message = message("import", "share");
            put(message, "kind", "text");
            put(message, "text", text);
            post(message, false);
            return;
        }
        postFailed("share", "分享的内容为空或格式不支持", false);
    }

    static void handleActivityResult(Activity activity, int requestCode, int resultCode, Intent data)
    {
        final Context context = activity.getApplicationContext();
        if (requestCode == REQUEST_PICK_IMAGES) {
            if (resultCode != Activity.RESULT_OK || data == null) {
                post(message("cancelled", "gallery"), true);
                return;
            }
            final ArrayList<Uri> uris = new ArrayList<>();
            ClipData clip = data.getClipData();
            if (clip != null) {
                for (int i = 0; i < clip.getItemCount(); ++i) {
                    if (clip.getItemAt(i).getUri() != null)
                        uris.add(clip.getItemAt(i).getUri());
                }
            } else if (data.getData() != null) {
                uris.add(data.getData());
            }
            if (uris.isEmpty()) {
                post(message("cancelled", "gallery"), true);
                return;
            }
            worker.execute(new Runnable() {
                @Override
                public void run()
                {
                    importUris(context, "gallery", uris, null, true);
                }
            });
        } else if (requestCode == REQUEST_CAPTURE_RECEIPT) {
            final String path = capturePath;
            capturePath = null;
            if (path == null) {
                postFailed("camera", "找不到相机照片", true);
                return;
            }
            final File file = new File(path);
            revokeCaptureGrant(activity, file);
            if (resultCode != Activity.RESULT_OK) {
                file.delete();
                post(message("cancelled", "camera"), true);
                return;
            }
            final long capturedAt = System.currentTimeMillis();
            worker.execute(new Runnable() {
                @Override
                public void run()
                {
                    importCapture(context, file, capturedAt);
                }
            });
        }
    }

    // ---- Starting system UI ----

    // Duplicate requests get an immediate failure so the caller always sees a terminal signal.
    private static boolean beginRequest(String origin)
    {
        if (requestInFlight.compareAndSet(false, true))
            return true;
        postFailed(origin, "正在处理上一次导入", false);
        return false;
    }

    private static void startPicker(Activity activity)
    {
        if (Build.VERSION.SDK_INT >= 33) {
            Intent intent = new Intent(MediaStore.ACTION_PICK_IMAGES);
            intent.setType("image/*");
            intent.putExtra(MediaStore.EXTRA_PICK_IMAGES_MAX,
                            Math.min(MediaStore.getPickImagesMaxLimit(), MAX_IMAGES_PER_IMPORT));
            try {
                activity.startActivityForResult(intent, REQUEST_PICK_IMAGES);
                return;
            } catch (ActivityNotFoundException e) {
                Log.w(TAG, "photo picker unavailable, falling back to GET_CONTENT");
            } catch (RuntimeException e) {
                Log.w(TAG, "photo picker failed: " + e.getClass().getSimpleName());
            }
        }
        Intent intent = new Intent(Intent.ACTION_GET_CONTENT);
        intent.setType("image/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        try {
            activity.startActivityForResult(intent, REQUEST_PICK_IMAGES);
        } catch (ActivityNotFoundException e) {
            postFailed("gallery", "没有可用的相册", true);
        } catch (RuntimeException e) {
            Log.w(TAG, "could not open the gallery: " + e.getClass().getSimpleName());
            postFailed("gallery", "无法打开相册", true);
        }
    }

    private static void startCamera(Activity activity)
    {
        File dir = new File(activity.getFilesDir(), CAPTURES_DIR);
        if (!dir.isDirectory() && !dir.mkdirs()) {
            postFailed("camera", "无法准备照片文件", true);
            return;
        }
        // Leftovers from a camera session that never returned.
        File[] stale = dir.listFiles();
        if (stale != null) {
            for (File file : stale)
                file.delete();
        }

        File file = new File(dir, UUID.randomUUID() + ".jpg");
        Uri uri;
        try {
            uri = captureUri(activity, file);
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "FileProvider is not configured for captures");
            postFailed("camera", "无法准备照片文件", true);
            return;
        }
        Intent intent = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);
        intent.putExtra(MediaStore.EXTRA_OUTPUT, uri);
        intent.setClipData(ClipData.newRawUri("", uri));
        // The camera only writes the photo; we read it back through the file path, not the URI.
        intent.addFlags(Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        try {
            activity.startActivityForResult(intent, REQUEST_CAPTURE_RECEIPT);
            capturePath = file.getPath();
        } catch (ActivityNotFoundException e) {
            revokeCaptureGrant(activity, file);
            postFailed("camera", "没有可用的相机", true);
        } catch (RuntimeException e) {
            Log.w(TAG, "could not open the camera: " + e.getClass().getSimpleName());
            revokeCaptureGrant(activity, file);
            postFailed("camera", "无法打开相机", true);
        }
    }

    private static Uri captureUri(Context context, File file)
    {
        return FileProvider.getUriForFile(context, context.getPackageName() + ".fileprovider", file);
    }

    private static void revokeCaptureGrant(Context context, File file)
    {
        try {
            context.revokeUriPermission(captureUri(context, file),
                                        Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (RuntimeException e) {
            Log.w(TAG, "could not revoke the camera grant: " + e.getClass().getSimpleName());
        }
    }

    // ---- Importing (worker thread) ----

    private static void importUris(Context context, String origin, ArrayList<Uri> uris, String fallbackType,
                                   boolean endsRequest)
    {
        JSONArray images = new JSONArray();
        JSONArray failures = new JSONArray();
        long budget = MAX_IMPORT_BYTES;
        for (int i = 0; i < uris.size(); ++i) {
            if (i >= MAX_IMAGES_PER_IMPORT) {
                failures.put("单次最多导入 " + MAX_IMAGES_PER_IMPORT + " 张");
                continue;
            }
            if (budget <= 0) {
                failures.put("本次导入超过 200 MB");
                continue;
            }
            try {
                File target = newImportFile(context);
                images.put(copyWithTimeout(context, uris.get(i), fallbackType, target, budget));
                budget -= target.length();
            } catch (ImportException e) {
                failures.put(e.getMessage());
            } catch (IOException e) {
                Log.w(TAG, "could not prepare an import file: " + e.getClass().getSimpleName());
                failures.put("无法保存图片");
            }
        }
        postImages(origin, images, failures, endsRequest);
    }

    private static JSONObject copyWithTimeout(final Context context, final Uri uri, final String fallbackType,
                                              final File target, final long budget) throws ImportException
    {
        Future<JSONObject> future = readers.submit(new Callable<JSONObject>() {
            @Override
            public JSONObject call() throws Exception
            {
                return copyImage(context, uri, fallbackType, target, budget);
            }
        });
        try {
            return future.get(READ_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            // A stuck reader may still recreate the .part file later; startup cleanup removes it.
            future.cancel(true);
            target.delete();
            throw new ImportException("读取图片超时");
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            future.cancel(true);
            target.delete();
            throw new ImportException("导入被中断");
        } catch (ExecutionException e) {
            Throwable cause = e.getCause();
            if (cause instanceof ImportException)
                throw (ImportException) cause;
            Log.w(TAG, "could not import an image: " + (cause == null ? "unknown" : cause.getClass().getSimpleName()));
            throw new ImportException("无法读取图片");
        }
    }

    private static void importCapture(Context context, File file, long capturedAt)
    {
        JSONArray images = new JSONArray();
        JSONArray failures = new JSONArray();
        File target = null;
        try {
            if (!file.isFile() || file.length() == 0)
                throw new ImportException("相机没有返回照片");
            target = newImportFile(context);
            if (!file.renameTo(target))
                throw new ImportException("无法保存照片");
            String mimeType = requireImage(target);
            JSONObject item = new JSONObject();
            put(item, "path", target.getPath());
            put(item, "mimeType", mimeType != null ? mimeType : "image/jpeg");
            put(item, "cameraCaptureMs", capturedAt);
            readExif(target, item);
            images.put(item);
            target = null;
        } catch (ImportException e) {
            failures.put(e.getMessage());
        } catch (IOException e) {
            failures.put("无法保存照片");
        } finally {
            if (target != null)
                target.delete();
            file.delete();
        }
        postImages("camera", images, failures, true);
    }

    private static JSONObject copyImage(Context context, Uri uri, String fallbackType, File target, long budget)
            throws ImportException, IOException
    {
        requireForeignContentUri(context, uri);

        ContentResolver resolver = context.getContentResolver();
        String mimeType = resolver.getType(uri);
        if (mimeType == null || mimeType.isEmpty())
            mimeType = fallbackType;
        if (mimeType != null && !mimeType.startsWith("image/"))
            throw new ImportException("不是图片");

        long limit = Math.min(MAX_IMAGE_BYTES, budget);
        String tooLarge = limit < MAX_IMAGE_BYTES ? "本次导入超过 200 MB" : "图片超过 50 MB";
        JSONObject item = new JSONObject();
        long size = queryMetadata(resolver, uri, item);
        if (size > limit)
            throw new ImportException(tooLarge);

        copy(resolver, uri, target, limit, tooLarge);
        String decodedType;
        try {
            decodedType = requireImage(target);
        } catch (ImportException e) {
            target.delete();
            throw e;
        }
        if (decodedType != null && (mimeType == null || mimeType.endsWith("/*")))
            mimeType = decodedType;
        readExif(target, item);
        put(item, "path", target.getPath());
        put(item, "mimeType", mimeType == null ? "" : mimeType);
        return item;
    }

    // Only content:// from other apps. file:// could point into our private storage, and so could
    // any provider of our own package (a malicious share would make us upload our own files).
    // "content://0@<authority>/" addresses another user's provider; getAuthority() keeps the
    // "0@" prefix, so the provider is resolved from the bare host and anything else is refused.
    private static void requireForeignContentUri(Context context, Uri uri) throws ImportException
    {
        if (!ContentResolver.SCHEME_CONTENT.equals(uri.getScheme()))
            throw new ImportException("只能导入 content:// 图片");
        String authority = uri.getAuthority();
        String host = uri.getHost();
        if (authority == null || host == null || !authority.equals(host) || authority.indexOf('@') >= 0)
            throw new ImportException("无效的图片地址");
        ProviderInfo provider = context.getPackageManager().resolveContentProvider(host, 0);
        if (provider != null && context.getPackageName().equals(provider.packageName))
            throw new ImportException("拒绝读取本应用自身的文件");
    }

    private static File newImportFile(Context context) throws IOException
    {
        File dir = new File(context.getFilesDir(), IMPORTS_DIR);
        if (!dir.isDirectory() && !dir.mkdirs())
            throw new IOException("cannot create the imports directory");
        // C++ renames it to <uuid>.<ext> once the MIME type is settled (import-files.cpp).
        return new File(dir, UUID.randomUUID() + ".part");
    }

    private static void copy(ContentResolver resolver, Uri uri, File target, long limit, String tooLarge)
            throws ImportException, IOException
    {
        long total = 0;
        try (InputStream in = resolver.openInputStream(uri);
             OutputStream out = new FileOutputStream(target)) {
            if (in == null)
                throw new IOException("provider returned no stream");
            byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                if (Thread.currentThread().isInterrupted())
                    throw new ImportException("读取图片超时");
                total += read;
                if (total > limit)
                    throw new ImportException(tooLarge);
                out.write(buffer, 0, read);
            }
            if (total == 0)
                throw new ImportException("图片为空");
        } catch (ImportException | IOException | RuntimeException e) {
            target.delete();
            throw e;
        }
    }

    // Returns the decoded MIME type (may be null); throws if the file is not a decodable image.
    private static String requireImage(File file) throws ImportException
    {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeFile(file.getPath(), options);
        if (options.outWidth <= 0 || options.outHeight <= 0)
            throw new ImportException("无法识别的图片格式");
        return options.outMimeType;
    }

    // Returns the reported size in bytes, or -1.
    private static long queryMetadata(ContentResolver resolver, Uri uri, JSONObject item)
    {
        try (Cursor cursor = resolver.query(uri, null, null, null, null)) {
            if (cursor == null || !cursor.moveToFirst())
                return -1;
            int nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
            if (nameIndex >= 0 && !cursor.isNull(nameIndex))
                put(item, "displayName", cursor.getString(nameIndex));
            long taken = longColumn(cursor, MediaStore.Images.ImageColumns.DATE_TAKEN);
            if (taken > 0)
                put(item, "mediaDateTakenMs", taken);
            long added = longColumn(cursor, MediaStore.MediaColumns.DATE_ADDED);
            if (added > 0)
                put(item, "mediaDateAddedMs", added * 1000);
            long modified = longColumn(cursor, MediaStore.MediaColumns.DATE_MODIFIED);
            if (modified > 0)
                put(item, "fileModifiedMs", modified * 1000);
            else if ((modified = longColumn(cursor, "last_modified")) > 0) // DocumentsContract, already ms
                put(item, "fileModifiedMs", modified);
            return longColumn(cursor, OpenableColumns.SIZE);
        } catch (RuntimeException e) {
            Log.w(TAG, "could not query image metadata: " + e.getClass().getSimpleName());
            return -1;
        }
    }

    private static long longColumn(Cursor cursor, String column)
    {
        int index = cursor.getColumnIndex(column);
        if (index < 0 || cursor.isNull(index))
            return -1;
        try {
            return cursor.getLong(index);
        } catch (RuntimeException e) {
            return -1;
        }
    }

    private static void readExif(File file, JSONObject item)
    {
        try {
            ExifInterface exif = new ExifInterface(file.getPath());
            String dateTime = exif.getAttribute(ExifInterface.TAG_DATETIME_ORIGINAL);
            if (dateTime != null)
                put(item, "exifDateTimeOriginal", dateTime);
            String offset = exif.getAttribute(ExifInterface.TAG_OFFSET_TIME_ORIGINAL);
            if (offset != null)
                put(item, "exifOffsetTimeOriginal", offset);
        } catch (IOException | RuntimeException e) {
            // Screenshots usually carry no EXIF; the other date sources take over.
        }
    }

    @SuppressWarnings("deprecation")
    private static ArrayList<Uri> streamUris(Intent intent)
    {
        ArrayList<Uri> uris = new ArrayList<>();
        try {
            if (Intent.ACTION_SEND_MULTIPLE.equals(intent.getAction())) {
                ArrayList<Uri> list = intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM);
                if (list != null) {
                    for (Uri uri : list) {
                        if (uri != null)
                            uris.add(uri);
                    }
                }
            } else {
                Uri uri = intent.getParcelableExtra(Intent.EXTRA_STREAM);
                if (uri != null)
                    uris.add(uri);
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "malformed EXTRA_STREAM: " + e.getClass().getSimpleName());
        }
        ClipData clip = intent.getClipData();
        if (uris.isEmpty() && clip != null) {
            for (int i = 0; i < clip.getItemCount(); ++i) {
                if (clip.getItemAt(i).getUri() != null)
                    uris.add(clip.getItemAt(i).getUri());
            }
        }
        return uris;
    }

    private static String sharedText(Intent intent)
    {
        CharSequence text = intent.getCharSequenceExtra(Intent.EXTRA_TEXT);
        if (text == null || text.toString().trim().isEmpty())
            return null;
        String body = text.toString();
        // Shopping apps often put the product or order title into the subject only.
        CharSequence subject = intent.getCharSequenceExtra(Intent.EXTRA_SUBJECT);
        if (subject != null && !subject.toString().trim().isEmpty() && !body.contains(subject))
            return subject + "\n" + body;
        return body;
    }

    // ---- Messages ----

    private static void runOnUi(String origin, final ActivityTask task)
    {
        final Activity activity = activityRef.get();
        if (activity == null) {
            if (origin != null)
                postFailed(origin, "界面尚未就绪", true);
            return;
        }
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run()
            {
                task.run(activity);
            }
        });
    }

    private static JSONObject message(String event, String origin)
    {
        JSONObject message = new JSONObject();
        put(message, "event", event);
        put(message, "origin", origin);
        return message;
    }

    private static void postImages(String origin, JSONArray images, JSONArray failures, boolean endsRequest)
    {
        JSONObject message = message("import", origin);
        put(message, "kind", "images");
        put(message, "images", images);
        put(message, "failures", failures);
        post(message, endsRequest);
    }

    private static void postFailed(String origin, String reason, boolean endsRequest)
    {
        JSONObject message = message("failed", origin);
        put(message, "message", reason);
        post(message, endsRequest);
    }

    // endsRequest: this is the terminal message of the open gallery / camera request.
    private static void post(JSONObject message, boolean endsRequest)
    {
        if (endsRequest)
            requestInFlight.set(false);
        String json = message.toString();
        synchronized (lock) {
            if (nativeReady)
                nativeOnMessage(json);
            else
                pending.add(json);
        }
    }

    private static void put(JSONObject object, String key, Object value)
    {
        try {
            object.put(key, value);
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }
    }
}
