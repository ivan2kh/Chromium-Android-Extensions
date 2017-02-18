// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.crash;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.webkit.ValueCallback;

import org.chromium.android_webview.PlatformServiceBridge;
import org.chromium.android_webview.command_line.CommandLineUtil;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.minidump_uploader.CrashFileManager;
import org.chromium.components.minidump_uploader.MinidumpUploadCallable;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

import java.io.File;

/**
 * Class in charge of uploading Minidumps from WebView's data directory.
 * This class gets invoked from a JobScheduler job and posts the operation of uploading minidumps to
 * a privately defined worker thread.
 * Note that this implementation is state-less in the sense that it doesn't keep track of whether it
 * successfully uploaded any minidumps. At the end of a job it simply checks whether there are any
 * minidumps left to upload, and if so, the job is rescheduled.
 */
public class MinidumpUploaderImpl implements MinidumpUploader {
    private static final String TAG = "MinidumpUploaderImpl";
    private Context mContext;
    private Thread mWorkerThread;
    private final ConnectivityManager mConnectivityManager;
    private final CrashFileManager mFileManager;

    private boolean mCancelUpload = false;

    private final boolean mCleanOutMinidumps;
    private boolean mPermittedByUser = false;

    @VisibleForTesting
    public static final int MAX_UPLOAD_TRIES_ALLOWED = 3;

    /**
     * Notify the worker thread that the current job has been canceled - so we shouldn't upload any
     * more minidumps.
     */
    private void setCancelUpload(boolean cancel) {
        mCancelUpload = cancel;
    }

    /**
     * Check whether the current job has been canceled.
     */
    private boolean getCancelUpload() {
        return mCancelUpload;
    }

    @VisibleForTesting
    public MinidumpUploaderImpl(Context context, boolean cleanOutMinidumps) {
        mConnectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        mContext = context;
        File webviewCrashDir = CrashReceiverService.createWebViewCrashDir(context);
        mFileManager = new CrashFileManager(webviewCrashDir);
        if (!mFileManager.ensureCrashDirExists()) {
            Log.e(TAG, "Crash directory doesn't exist!");
        }
        mCleanOutMinidumps = cleanOutMinidumps;
    }

    /**
     * Utility method to allow us to test the logic of this class by injecting
     * test-MinidumpUploadCallables.
     */
    @VisibleForTesting
    public MinidumpUploadCallable createMinidumpUploadCallable(File minidumpFile, File logfile) {
        return new MinidumpUploadCallable(
                minidumpFile, logfile, createWebViewCrashReportingManager());
    }

    /**
     * Utility method to allow us to test the logic of this class by injecting
     * a test-PlatformServiceBridge.
     */
    @VisibleForTesting
    public PlatformServiceBridge createPlatformServiceBridge() {
        return PlatformServiceBridge.getInstance(mContext);
    }

    @VisibleForTesting
    protected CrashReportingPermissionManager createWebViewCrashReportingManager() {
        return new CrashReportingPermissionManager() {
            @Override
            public boolean isClientInMetricsSample() {
                // We will check whether the client is in the metrics sample before
                // generating a minidump - so if no minidump is generated this code will
                // never run and we don't need to check whether we are in the sample.
                // TODO(gsennton): when we switch to using Finch for this value we should use the
                // Finch value here as well.
                return true;
            }
            @Override
            public boolean isNetworkAvailableForCrashUploads() {
                // JobScheduler will call onStopJob causing our upload to be interrupted when our
                // network requirements no longer hold.
                NetworkInfo networkInfo = mConnectivityManager.getActiveNetworkInfo();
                if (networkInfo == null || !networkInfo.isConnected()) return false;
                return !mConnectivityManager.isActiveNetworkMetered();
            }
            @Override
            public boolean isCrashUploadDisabledByCommandLine() {
                return false;
            }
            /**
             * This method is already represented by isClientInMetricsSample() and
             * isNetworkAvailableForCrashUploads().
             */
            @Override
            public boolean isMetricsUploadPermitted() {
                return true;
            }
            @Override
            public boolean isUsageAndCrashReportingPermittedByUser() {
                return mPermittedByUser;
            }
            @Override
            public boolean isUploadEnabledForTests() {
                // Note that CommandLine/CommandLineUtil are not thread safe. They are initialized
                // on the main thread, but before the current worker thread started - so this thread
                // will have seen the initialization of the CommandLine.
                return CommandLine.getInstance().hasSwitch(
                        CommandLineUtil.CRASH_UPLOADS_ENABLED_FOR_TESTING_SWITCH);
            }
        };
    }

    /**
     * Runnable that upload minidumps.
     * This is where the actual uploading happens - an upload job consists of posting this Runnable
     * to the worker thread.
     */
    private class UploadRunnable implements Runnable {
        private final MinidumpUploader.UploadsFinishedCallback mUploadsFinishedCallback;

        public UploadRunnable(MinidumpUploader.UploadsFinishedCallback uploadsFinishedCallback) {
            mUploadsFinishedCallback = uploadsFinishedCallback;
        }

        @Override
        public void run() {
            File[] minidumps = mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED);
            for (File minidump : minidumps) {
                // Store the name of the current minidump so that we can mark it as a failure from
                // the main thread if JobScheduler tells us to stop.
                MinidumpUploadCallable uploadCallable = createMinidumpUploadCallable(
                        minidump, mFileManager.getCrashUploadLogFile());
                int uploadResult = uploadCallable.call();

                // Job was canceled -> early out: any clean-up will be performed in cancelUploads().
                // Note that we check whether we are canceled AFTER trying to upload a minidump -
                // this is to allow the uploading of at least one minidump per job (to deal with
                // cases where we reschedule jobs over and over again and would never upload any
                // minidumps because old jobs are canceled when scheduling new jobs).
                if (getCancelUpload()) return;

                if (uploadResult == MinidumpUploadCallable.UPLOAD_FAILURE) {
                    String newName = CrashFileManager.tryIncrementAttemptNumber(minidump);
                    if (newName == null) {
                        Log.w(TAG, "Failed to increment attempt number of " + minidump);
                    }
                }
            }

            // Clean out old/uploaded minidumps. Note that this clean-up method is more strict than
            // our copying mechanism in the sense that it keeps less minidumps.
            if (mCleanOutMinidumps) {
                mFileManager.cleanOutAllNonFreshMinidumpFiles();
            }

            // Reschedule if there are still minidumps to upload.
            boolean reschedule =
                    mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED).length > 0;
            mUploadsFinishedCallback.uploadsFinished(reschedule);
        }
    }

    @Override
    public void uploadAllMinidumps(
            final MinidumpUploader.UploadsFinishedCallback uploadsFinishedCallback) {
        // This call should happen on the main thread
        ThreadUtils.assertOnUiThread();
        if (mWorkerThread != null) {
            throw new RuntimeException("Only one upload-job should be active at a time");
        }
        mWorkerThread = new Thread(new UploadRunnable(uploadsFinishedCallback), "mWorkerThread");
        setCancelUpload(false);

        createPlatformServiceBridge().queryMetricsSetting(new ValueCallback<Boolean>() {
            public void onReceiveValue(Boolean enabled) {
                // This callback should be received on the main thread (e.g. mWorkerThread
                // is not thread-safe).
                ThreadUtils.assertOnUiThread();

                mPermittedByUser = enabled;
                // Note that our job might have been cancelled by this time - however, we do start
                // our worker thread anyway to try to make some progress towards uploading
                // minidumps.
                // This is to ensure that in the case where an app is crashing over and over again
                // - and we are rescheduling jobs over and over again - we are still uploading at
                // least one minidump per job, as long as that job starts before it is canceled by
                // the next job.
                // For cases where the job is cancelled because the network connection is lost, or
                // because we switch over to a metered connection, we won't try to upload any
                // minidumps anyway since we check the network connection just before the upload of
                // each minidump.
                mWorkerThread.start();
            }
        });
    }

    @VisibleForTesting
    public void joinWorkerThreadForTesting() throws InterruptedException {
        mWorkerThread.join();
    }

    /**
     * @return whether to reschedule the uploads.
     */
    @Override
    public boolean cancelUploads() {
        setCancelUpload(true);

        // Reschedule if there are still minidumps to upload.
        return mFileManager.getAllMinidumpFiles(MAX_UPLOAD_TRIES_ALLOWED).length > 0;
    }
}
