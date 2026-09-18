#pragma once

void klog(const char *fmt, ...);
void klog_install_crash_handler();
void klog_set_backend(const char *label);
