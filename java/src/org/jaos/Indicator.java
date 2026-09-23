// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The row holds only while integer column {@code col} equals {@code value}. */
public record Indicator(long col, int value) {}
