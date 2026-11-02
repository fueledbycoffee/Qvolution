"""Deadline-bound pipe reads, including children that outlive their launcher."""
import os
import selectors
import time


def chunks(stream, deadline, cancelled=None):
    os.set_blocking(stream.fileno(), False)
    with selectors.DefaultSelector() as selector:
        selector.register(stream, selectors.EVENT_READ)
        while time.monotonic() < deadline and not (cancelled and cancelled.is_set()):
            if not selector.select(min(.1, max(0, deadline - time.monotonic()))):
                continue
            try:
                data = os.read(stream.fileno(), 65536)
            except BlockingIOError:
                continue
            if not data:
                return
            yield data
    raise TimeoutError('pipe did not reach EOF before its deadline')


def capture(stream, deadline, limit=1024 * 1024):
    output = bytearray()
    try:
        for data in chunks(stream, deadline):
            if len(output) + len(data) > limit:
                raise ValueError('pipe output exceeded byte budget')
            output.extend(data)
        return bytes(output), True
    except TimeoutError:
        return bytes(output), False
    finally:
        stream.close()
