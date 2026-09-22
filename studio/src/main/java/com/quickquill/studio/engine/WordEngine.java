package com.quickquill.studio.engine;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;

/**
 * Java bridge to the QuickQuill C++ dictionary engine via Foreign Function and Memory API (Panama
 * FFM). Each method resolves a native C function from libquickquill_engine.so, allocates a confined
 * arena for the call, and returns the JSON response as a Java String.
 */
public class WordEngine {

  private static final int BUF_SIZE = 131072; // 128KB — generous buffer for any JSON response

  private static final Linker LINKER = Linker.nativeLinker();
  private static final SymbolLookup LOOKUP = SymbolLookup.loaderLookup();

  // Downcall handles — resolved once at class-load time so every
  // subsequent call skips symbol lookup overhead.
  private static MethodHandle INIT_HANDLE;
  private static MethodHandle SHUTDOWN_HANDLE;
  private static MethodHandle LOOKUP_HANDLE;
  private static MethodHandle SUGGEST_HANDLE;
  private static MethodHandle SYNONYM_HANDLE;
  private static MethodHandle AUTOFILL_HANDLE;
  private static MethodHandle WOTD_HANDLE;

  static {
    try {
      INIT_HANDLE =
          LINKER.downcallHandle(
              LOOKUP.find("qq_init").orElseThrow(),
              FunctionDescriptor.of(ValueLayout.JAVA_INT, ValueLayout.ADDRESS));

      SHUTDOWN_HANDLE =
          LINKER.downcallHandle(
              LOOKUP.find("qq_shutdown").orElseThrow(), FunctionDescriptor.ofVoid());

      LOOKUP_HANDLE =
          LINKER.downcallHandle(
              LOOKUP.find("qq_lookup").orElseThrow(),
              FunctionDescriptor.of(
                  ValueLayout.JAVA_INT,
                  ValueLayout.ADDRESS,
                  ValueLayout.ADDRESS,
                  ValueLayout.JAVA_INT));

      SUGGEST_HANDLE =
          LINKER.downcallHandle(
              LOOKUP.find("qq_suggest").orElseThrow(),
              FunctionDescriptor.of(
                  ValueLayout.JAVA_INT,
                  ValueLayout.ADDRESS,
                  ValueLayout.ADDRESS,
                  ValueLayout.JAVA_INT));

      SYNONYM_HANDLE =
          LINKER.downcallHandle(
              LOOKUP.find("qq_synonym").orElseThrow(),
              FunctionDescriptor.of(
                  ValueLayout.JAVA_INT,
                  ValueLayout.ADDRESS,
                  ValueLayout.ADDRESS,
                  ValueLayout.JAVA_INT));

      AUTOFILL_HANDLE =
          LINKER.downcallHandle(
              LOOKUP.find("qq_autofill").orElseThrow(),
              FunctionDescriptor.of(
                  ValueLayout.JAVA_INT,
                  ValueLayout.ADDRESS,
                  ValueLayout.ADDRESS,
                  ValueLayout.ADDRESS,
                  ValueLayout.ADDRESS,
                  ValueLayout.JAVA_INT));

      WOTD_HANDLE =
          LINKER.downcallHandle(
              LOOKUP.find("qq_wotd").orElseThrow(),
              FunctionDescriptor.of(
                  ValueLayout.JAVA_INT,
                  ValueLayout.JAVA_LONG,
                  ValueLayout.ADDRESS,
                  ValueLayout.JAVA_INT));
    } catch (Exception e) {
      throw new RuntimeException("Failed to link quickquill native library", e);
    }
  }

  /** Initialize the C++ engine with the given SQLite database path. */
  public static void init(String dbPath) {
    try (Arena arena = Arena.ofConfined()) {
      int result = (int) INIT_HANDLE.invoke(arena.allocateFrom(dbPath));
      if (result != 0) {
        throw new RuntimeException("qq_init returned error code " + result);
      }
    } catch (Throwable e) {
      throw new RuntimeException("Failed to initialize word engine", e);
    }
  }

  /** Shut down the C++ engine and free all native resources. */
  public static void shutdown() {
    try {
      SHUTDOWN_HANDLE.invoke();
    } catch (Throwable e) {
      throw new RuntimeException("Failed to shut down word engine", e);
    }
  }

  /** Dictionary lookup — returns JSON with lemma, senses, forms, etymology. */
  public static String lookup(String word) {
    return callWithTwoArgs(LOOKUP_HANDLE, word);
  }

  /** Spelling suggestions — returns JSON array of similar words. */
  public static String suggest(String word) {
    return callWithTwoArgs(SUGGEST_HANDLE, word);
  }

  /** Synonym suggestions — returns JSON array of random synonyms. */
  public static String synonym(String word) {
    return callWithTwoArgs(SYNONYM_HANDLE, word);
  }

  /**
   * Autocomplete — returns JSON with best completion for the prefix. historyJson and suggestedJson
   * are JSON array strings, e.g. ["word1","word2"].
   */
  public static String autofill(String prefix, String historyJson, String suggestedJson) {
    try (Arena arena = Arena.ofConfined()) {
      MemorySegment buf = arena.allocate(BUF_SIZE);
      int len =
          (int)
              AUTOFILL_HANDLE.invoke(
                  arena.allocateFrom(prefix),
                  arena.allocateFrom(historyJson != null ? historyJson : "[]"),
                  arena.allocateFrom(suggestedJson != null ? suggestedJson : "[]"),
                  buf,
                  BUF_SIZE);
      return len >= 0 ? readString(buf, len) : "";
    } catch (Throwable e) {
      throw new RuntimeException("autofill failed", e);
    }
  }

  /**
   * Deterministic per-day word. dayNumber is typically LocalDate.toEpochDay(); the same day always
   * maps to the same dictionary entry.
   */
  public static String wordOfTheDay(long dayNumber) {
    try (Arena arena = Arena.ofConfined()) {
      MemorySegment buf = arena.allocate(BUF_SIZE);
      int len = (int) WOTD_HANDLE.invoke(dayNumber, buf, BUF_SIZE);
      return len >= 0 ? readString(buf, len) : "";
    } catch (Throwable e) {
      throw new RuntimeException("word-of-the-day failed", e);
    }
  }

  /** Helper: call a two-arg native function (word + output buffer). */
  private static String callWithTwoArgs(MethodHandle handle, String arg) {
    try (Arena arena = Arena.ofConfined()) {
      MemorySegment buf = arena.allocate(BUF_SIZE);
      int len = (int) handle.invoke(arena.allocateFrom(arg), buf, BUF_SIZE);
      return len >= 0 ? readString(buf, len) : "";
    } catch (Throwable e) {
      throw new RuntimeException("Native call failed", e);
    }
  }

  /**
   * Reads the NUL-terminated string written by the engine. The engine truncates at {@code BUF_SIZE
   * - 1} bytes; when the return value reached that limit the JSON was cut off, so we fail loudly
   * instead of silently returning incomplete data.
   */
  private static String readString(MemorySegment buf, int len) {
    if (len >= BUF_SIZE - 1) {
      throw new RuntimeException("Engine response exceeded buffer size " + BUF_SIZE);
    }
    return buf.getString(0);
  }
}
