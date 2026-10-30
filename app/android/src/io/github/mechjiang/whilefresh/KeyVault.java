package io.github.mechjiang.whilefresh;

import android.content.Context;
import android.util.Log;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;

// Encrypted storage for the user's API keys (R-ADR-3), called from app/src/platform/android/. The
// AES-GCM key lives in AndroidKeyStore and never leaves it; each secret is a file in app-private
// storage holding the 12-byte IV followed by the ciphertext. Values are never logged.
public final class KeyVault
{
    private static final String TAG = "WhileFresh";
    private static final String KEY_ALIAS = "whilefresh-secrets";
    private static final String KEYSTORE = "AndroidKeyStore";
    private static final String DIR = "secrets";
    private static final int IV_BYTES = 12;
    private static final int TAG_BITS = 128;
    private static final Object lock = new Object();

    private KeyVault() {}

    public static boolean put(Context context, String name, String value)
    {
        synchronized (lock) {
            File target = file(context, name);
            File temp = new File(target.getParentFile(), name + ".tmp");
            try {
                File dir = target.getParentFile();
                if (!dir.isDirectory() && !dir.mkdirs())
                    return false;
                Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
                cipher.init(Cipher.ENCRYPT_MODE, key(true));
                byte[] iv = cipher.getIV();
                byte[] sealed = cipher.doFinal(value.getBytes(StandardCharsets.UTF_8));
                try (FileOutputStream out = new FileOutputStream(temp)) {
                    out.write(iv);
                    out.write(sealed);
                    out.getFD().sync();
                }
                if (!temp.renameTo(target)) {
                    temp.delete();
                    return false;
                }
                return true;
            } catch (Exception e) {
                temp.delete();
                Log.w(TAG, "secret store: could not save a value: " + e.getClass().getSimpleName());
                return false;
            }
        }
    }

    // Null when the secret is absent or cannot be decrypted (e.g. the Keystore key was lost).
    public static String get(Context context, String name)
    {
        synchronized (lock) {
            File source = file(context, name);
            if (!source.isFile())
                return null;
            try {
                byte[] data = new byte[(int) source.length()];
                try (FileInputStream in = new FileInputStream(source)) {
                    int read = 0;
                    while (read < data.length) {
                        int n = in.read(data, read, data.length - read);
                        if (n < 0)
                            throw new IOException("short read");
                        read += n;
                    }
                }
                if (data.length <= IV_BYTES)
                    return null;
                SecretKey key = key(false);
                if (key == null)
                    return null;
                Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
                cipher.init(Cipher.DECRYPT_MODE, key, new GCMParameterSpec(TAG_BITS, Arrays.copyOfRange(data, 0, IV_BYTES)));
                return new String(cipher.doFinal(data, IV_BYTES, data.length - IV_BYTES), StandardCharsets.UTF_8);
            } catch (Exception e) {
                Log.w(TAG, "secret store: could not read a value: " + e.getClass().getSimpleName());
                return null;
            }
        }
    }

    public static boolean remove(Context context, String name)
    {
        synchronized (lock) {
            File target = file(context, name);
            return !target.exists() || target.delete();
        }
    }

    private static File file(Context context, String name)
    {
        if (!name.matches("[a-z0-9-]+"))
            throw new IllegalArgumentException("bad secret name");
        return new File(new File(context.getFilesDir(), DIR), name + ".bin");
    }

    private static SecretKey key(boolean create) throws Exception
    {
        KeyStore store = KeyStore.getInstance(KEYSTORE);
        store.load(null);
        if (store.containsAlias(KEY_ALIAS))
            return (SecretKey) store.getKey(KEY_ALIAS, null);
        if (!create)
            return null;
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE);
        generator.init(new KeyGenParameterSpec.Builder(KEY_ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                           .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                           .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                           .setKeySize(256)
                           .build());
        return generator.generateKey();
    }
}
