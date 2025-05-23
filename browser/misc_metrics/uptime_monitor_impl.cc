/* Copyright (c) 2023 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/misc_metrics/uptime_monitor_impl.h"

#include "base/time/time.h"
#include "brave/components/misc_metrics/pref_names.h"
#include "brave/components/p3a_utils/bucket.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "brave/browser/misc_metrics/usage_clock.h"
#endif

namespace misc_metrics {

namespace {

#if !BUILDFLAG(IS_ANDROID)
constexpr base::TimeDelta kUsageTimeQueryInterval = base::Minutes(1);
#endif
constexpr base::TimeDelta kUsageTimeReportInterval = base::Days(1);

constexpr int kBrowserOpenTimeBuckets[] = {30, 60, 120, 180, 300, 420, 600};

}  // namespace

UptimeMonitorImpl::UptimeMonitorImpl(PrefService* local_state)
    : local_state_(local_state),
      report_frame_start_time_(
          local_state->GetTime(kDailyUptimeFrameStartTimePrefName)),
      report_frame_time_sum_(
          local_state_->GetTimeDelta(kDailyUptimeSumPrefName)),
      weekly_storage_(local_state, kWeeklyUptimeStoragePrefName) {}

void UptimeMonitorImpl::Init() {
  if (report_frame_start_time_.is_null()) {
    // If today is the first time monitoring uptime, set the frame start time
    // to now.
    ResetReportFrame();
  }
  RecordP3A();
#if !BUILDFLAG(IS_ANDROID)
  usage_clock_ = std::make_unique<UsageClock>();
  timer_.Start(FROM_HERE, kUsageTimeQueryInterval,
               base::BindRepeating(&UptimeMonitorImpl::RecordUsage,
                                   base::Unretained(this)));
#endif
}

#if BUILDFLAG(IS_ANDROID)
void UptimeMonitorImpl::ReportUsageDuration(base::TimeDelta duration) {
  report_frame_time_sum_ += duration;
  local_state_->SetTimeDelta(kDailyUptimeSumPrefName, report_frame_time_sum_);

  weekly_storage_.AddDelta(duration.InSeconds());

  RecordP3A();
}
#else
void UptimeMonitorImpl::RecordUsage() {
  const base::TimeDelta new_total = usage_clock_->GetTotalUsageTime();
  const base::TimeDelta total_diff = new_total - current_total_usage_;
  if (total_diff > base::TimeDelta()) {
    report_frame_time_sum_ += total_diff;
    current_total_usage_ = new_total;
    local_state_->SetTimeDelta(kDailyUptimeSumPrefName, report_frame_time_sum_);

    weekly_storage_.AddDelta(total_diff.InSeconds());
  }
  RecordP3A();
}
#endif

void UptimeMonitorImpl::RecordP3A() {
  if ((base::Time::Now() - report_frame_start_time_) <
      kUsageTimeReportInterval) {
    // Do not report, since 1 day has not passed.
    return;
  }
  p3a_utils::RecordToHistogramBucket(kBrowserOpenTimeHistogramName,
                                     kBrowserOpenTimeBuckets,
                                     report_frame_time_sum_.InMinutes());
  ResetReportFrame();
}

void UptimeMonitorImpl::ResetReportFrame() {
  report_frame_time_sum_ = base::TimeDelta();
  report_frame_start_time_ = base::Time::Now();
  local_state_->SetTimeDelta(kDailyUptimeSumPrefName, report_frame_time_sum_);
  local_state_->SetTime(kDailyUptimeFrameStartTimePrefName,
                        report_frame_start_time_);
}

base::TimeDelta UptimeMonitorImpl::GetUsedTimeInWeek() const {
  return base::Seconds(weekly_storage_.GetWeeklySum());
}

base::WeakPtr<UptimeMonitor> UptimeMonitorImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UptimeMonitorImpl::IsInUse() const {
#if !BUILDFLAG(IS_ANDROID)
  return usage_clock_->IsInUse();
#else
  return true;
#endif
}

UptimeMonitorImpl::~UptimeMonitorImpl() = default;

void UptimeMonitorImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimeDeltaPref(kDailyUptimeSumPrefName, base::TimeDelta());
  registry->RegisterTimePref(kDailyUptimeFrameStartTimePrefName, base::Time());
  registry->RegisterListPref(kWeeklyUptimeStoragePrefName);
}

void UptimeMonitorImpl::RegisterPrefsForMigration(
    PrefRegistrySimple* registry) {
  // Added 10/2023
  registry->RegisterListPref(kDailyUptimesListPrefName);
}

void UptimeMonitorImpl::MigrateObsoletePrefs(PrefService* local_state) {
  // Added 10/2023
  local_state->ClearPref(kDailyUptimesListPrefName);
}

}  // namespace misc_metrics
