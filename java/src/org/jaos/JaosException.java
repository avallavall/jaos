// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** A C call that failed, with the model's message. */
public final class JaosException extends RuntimeException {
    private static final long serialVersionUID = 1L;
    private final Status status;

    public JaosException(Status status, String message) {
        super(message.isEmpty() ? status.toString() : message);
        this.status = status;
    }

    public Status status() {
        return status;
    }
}
