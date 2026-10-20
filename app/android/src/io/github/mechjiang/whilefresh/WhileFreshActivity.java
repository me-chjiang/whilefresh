package io.github.mechjiang.whilefresh;

import android.content.Intent;
import android.os.Bundle;

import org.qtproject.qt.android.bindings.QtActivity;

// launchMode="singleTask": a second QtActivity in a running process makes Qt restart the whole app,
// so shares from other apps must reach this instance through onNewIntent instead.
public class WhileFreshActivity extends QtActivity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        WhileFreshBridge.onActivityCreated(this, savedInstanceState);
        // A restored activity would see the intent that launched it the first time.
        if (savedInstanceState == null)
            WhileFreshBridge.handleIntent(this, getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent)
    {
        super.onNewIntent(intent);
        setIntent(intent);
        WhileFreshBridge.handleIntent(this, intent);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data)
    {
        super.onActivityResult(requestCode, resultCode, data);
        WhileFreshBridge.handleActivityResult(this, requestCode, resultCode, data);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState)
    {
        super.onSaveInstanceState(outState);
        WhileFreshBridge.onSaveInstanceState(outState);
    }

    @Override
    protected void onDestroy()
    {
        WhileFreshBridge.onActivityDestroyed(this);
        super.onDestroy();
    }
}
