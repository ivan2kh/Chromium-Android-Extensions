// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/policy/android_management_client.h"
#include "components/arc/arc_session_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

class ArcAppLauncher;
class Profile;

namespace ash {
class ShelfDelegate;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace arc {

class ArcAndroidManagementChecker;
class ArcAuthInfoFetcher;
class ArcAuthContext;
class ArcSessionRunner;
class ArcTermsOfServiceNegotiator;
enum class ProvisioningResult : int;

// This class proxies the request from the client to fetch an auth code from
// LSO. It lives on the UI thread.
class ArcSessionManager : public ArcSessionObserver,
                          public ArcSupportHost::Observer,
                          public sync_preferences::PrefServiceSyncableObserver {
 public:
  // Represents each State of ARC session.
  // NOT_INITIALIZED: represents the state that the Profile is not yet ready
  //   so that this service is not yet initialized, or Chrome is being shut
  //   down so that this is destroyed.
  // STOPPED: ARC session is not running, or being terminated.
  // SHOWING_TERMS_OF_SERVICE: "Terms Of Service" page is shown on ARC support
  //   Chrome app.
  // CHECKING_ANDROID_MANAGEMENT: Checking Android management status. Note that
  //   the status is checked for each ARC session starting, but this is the
  //   state only for the first boot case (= opt-in case). The second time and
  //   later the management check is running in parallel with ARC session
  //   starting, and in such a case, State is ACTIVE, instead.
  // REMOVING_DATA_DIR: When ARC is disabled, the data directory is removed.
  //   While removing is processed, ARC cannot be started. This is the state.
  // ACTIVE: ARC is running.
  //
  // State transition should be as follows:
  //
  // NOT_INITIALIZED -> STOPPED: when the primary Profile gets ready.
  // ...(any)... -> NOT_INITIALIZED: when the Chrome is being shutdown.
  // ...(any)... -> STOPPED: on error.
  //
  // In the first boot case:
  //   STOPPED -> SHOWING_TERMS_OF_SERVICE: when arc.enabled preference is set.
  //   SHOWING_TERMS_OF_SERVICE -> CHECKING_ANDROID_MANAGEMENT: when a user
  //     agree with "Terms Of Service"
  //   CHECKING_ANDROID_MANAGEMENT -> ACTIVE: when the auth token is
  //     successfully fetched.
  //
  // In the second (or later) boot case:
  //   STOPPED -> ACTIVE: when arc.enabled preference is checked that it is
  //     true. Practically, this is when the primary Profile gets ready.
  //
  // TODO(hidehiko): Fix the state machine, and update the comment including
  // relationship with |enable_requested_|.
  enum class State {
    NOT_INITIALIZED,
    STOPPED,
    SHOWING_TERMS_OF_SERVICE,
    CHECKING_ANDROID_MANAGEMENT,
    REMOVING_DATA_DIR,
    ACTIVE,
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called to notify that whether Google Play Store is enabled or not, which
    // is represented by "arc.enabled" preference, is updated.
    virtual void OnArcPlayStoreEnabledChanged(bool enabled) {}

    // Called to notify that ARC has been initialized successfully.
    virtual void OnArcInitialStart() {}

    // Called to notify that Android data has been removed. Used in
    // browser_tests
    virtual void OnArcDataRemoved() {}
  };

  explicit ArcSessionManager(
      std::unique_ptr<ArcSessionRunner> arc_session_runner);
  ~ArcSessionManager() override;

  static ArcSessionManager* Get();

  // Exposed here for unit_tests validation.
  static bool IsOobeOptInActive();

  // It is called from chrome/browser/prefs/browser_prefs.cc.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static void DisableUIForTesting();
  static void SetShelfDelegateForTesting(ash::ShelfDelegate* shelf_delegate);
  static void EnableCheckAndroidManagementForTesting();

  // Returns true if ARC is allowed to run for the current session.
  // TODO(hidehiko): The name is very close to IsArcAllowedForProfile(), but
  // has different meaning. Clean this up.
  bool IsAllowed() const;

  void OnPrimaryUserProfilePrepared(Profile* profile);
  void Shutdown();

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }

  State state() const { return state_; }

  // Adds or removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Adds or removes ArcSessionObservers.
  // TODO(hidehiko): The observer should be migrated into
  // ArcSessionManager::Observer.
  void AddSessionObserver(ArcSessionObserver* observer);
  void RemoveSessionObserver(ArcSessionObserver* observer);

  // Returns true if ARC instance is running/stopped, respectively.
  // See ArcSessionRunner::IsRunning()/IsStopped() for details.
  bool IsSessionRunning() const;
  bool IsSessionStopped() const;

  // Called from ARC support platform app when user cancels signing.
  void CancelAuthCode();

  // TODO(hidehiko): Better to rename longer but descriptive one, e.g.
  // IsArcEnabledPreferenceManaged.
  // TODO(hidehiko): Look at the real usage, and write document.
  bool IsArcManaged() const;

  // Returns the preference value of "arc.enabled", which means whether the
  // user has opted in (or is opting in now) to use Google Play Store on ARC.
  bool IsArcPlayStoreEnabled() const;

  // Enables/disables Google Play Store on ARC. Currently, it is tied to
  // ARC enabled state, too, so this also should trigger to enable/disable
  // whole ARC system.
  // TODO(hidehiko): De-couple the concept to enable ARC system and opt-in
  // to use Google Play Store. Note that there is a plan to use ARC without
  // Google Play Store, then ARC can run without opt-in.
  void SetArcPlayStoreEnabled(bool enable);

  // Requests to enable ARC session. This starts ARC instance, or maybe starts
  // Terms Of Service negotiation if they haven't been accepted yet.
  // If it is already requested to enable, no-op.
  // Currently, enabled/disabled is tied to whether Google Play Store is
  // enabled or disabled. Please see also TODO of SetArcPlayStoreEnabled().
  void RequestEnable();

  // Requests to disable ARC session. This stops ARC instance, or quits Terms
  // Of Service negotiation if it is the middle of the process (e.g. closing
  // UI for manual negotiation if it is shown).
  // If it is already requested to disable, no-op.
  void RequestDisable();

  // Called from the Chrome OS metrics provider to record Arc.State
  // periodically.
  void RecordArcState();

  // sync_preferences::PrefServiceSyncableObserver
  void OnIsSyncingChanged() override;

  // ArcSupportHost::Observer:
  void OnWindowClosed() override;
  void OnTermsAgreed(bool is_metrics_enabled,
                     bool is_backup_and_restore_enabled,
                     bool is_location_service_enabled) override;
  void OnRetryClicked() override;
  void OnSendFeedbackClicked() override;

  // Stops ARC without changing ArcEnabled preference.
  void StopArc();

  // StopArc(), then restart. Between them data clear may happens.
  // This is a special method to support enterprise device lost case.
  // This can be called only when ARC is running.
  void StopAndEnableArc();

  // Removes the data if ARC is stopped. Otherwise, queue to remove the data
  // on ARC is stopped. A log statement with the removal reason must be added
  // prior to calling RemoveArcData().
  void RemoveArcData();

  ArcSupportHost* support_host() { return support_host_.get(); }

  // TODO(hidehiko): Get rid of the getter by migration between ArcAuthContext
  // and ArcAuthInfoFetcher.
  ArcAuthContext* auth_context() { return context_.get(); }

  void StartArc();

  void OnProvisioningFinished(ProvisioningResult result);

  // Returns the time when the sign in process started, or a null time if
  // signing in didn't happen during this session.
  base::Time sign_in_start_time() const { return sign_in_start_time_; }

  // Returns the time when ARC was about to start, or a null time if ARC has not
  // been started yet.
  base::Time arc_start_time() const { return arc_start_time_; }

  // Injectors for testing.
  void SetArcSessionRunnerForTesting(
      std::unique_ptr<ArcSessionRunner> arc_session_runner);
  void SetAttemptUserExitCallbackForTesting(const base::Closure& callback);

 private:
  // RequestEnable() has a check in order not to trigger starting procedure
  // twice. This method can be called to bypass that check when restarting.
  void RequestEnableImpl();

  // Negotiates the terms of service to user.
  void StartTermsOfServiceNegotiation();
  void OnTermsOfServiceNegotiated(bool accepted);

  void SetState(State state);
  void ShutdownSession();
  void OnOptInPreferenceChanged();
  void OnAndroidManagementPassed();
  void OnArcDataRemoved(bool success);
  void OnArcSignInTimeout();
  void FetchAuthCode();
  void PrepareContextForAuthCodeRequest();

  void StartArcAndroidManagementCheck();
  void MaybeReenableArc();

  // Called when the Android management check is done in opt-in flow or
  // re-auth flow.
  void OnAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);

  // Called when the background Android management check is done. It is
  // triggered when the second or later ARC boot timing.
  void OnBackgroundAndroidManagementChecked(
      policy::AndroidManagementClient::Result result);

  // ArcSessionObserver:
  void OnSessionReady() override;
  void OnSessionStopped(StopReason reason) override;

  std::unique_ptr<ArcSessionRunner> arc_session_runner_;

  // Unowned pointer. Keeps current profile.
  Profile* profile_ = nullptr;

  // Registrar used to monitor ARC enabled state.
  PrefChangeRegistrar pref_change_registrar_;

  // Whether ArcSessionManager is requested to enable (starting to run ARC
  // instance) or not.
  bool enable_requested_ = false;

  // Internal state machine. See also State enum class.
  State state_ = State::NOT_INITIALIZED;
  base::ObserverList<Observer> observer_list_;
  base::ObserverList<ArcSessionObserver> arc_session_observer_list_;
  std::unique_ptr<ArcAppLauncher> playstore_launcher_;
  bool reenable_arc_ = false;
  bool provisioning_reported_ = false;
  base::OneShotTimer arc_sign_in_timer_;

  std::unique_ptr<ArcSupportHost> support_host_;

  std::unique_ptr<ArcTermsOfServiceNegotiator> terms_of_service_negotiator_;

  std::unique_ptr<ArcAuthContext> context_;
  std::unique_ptr<ArcAndroidManagementChecker> android_management_checker_;

  // The time when the sign in process started.
  base::Time sign_in_start_time_;
  // The time when ARC was about to start.
  base::Time arc_start_time_;
  base::Closure attempt_user_exit_callback_;

  // Must be the last member.
  base::WeakPtrFactory<ArcSessionManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManager);
};

// Outputs the stringified |state| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os,
                         const ArcSessionManager::State& state);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_SESSION_MANAGER_H_
