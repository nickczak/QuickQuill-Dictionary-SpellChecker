package com.quickquill.studio.controller;

import com.quickquill.studio.engine.WordEngine;
import java.time.LocalDate;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import tools.jackson.databind.JsonNode;
import tools.jackson.databind.json.JsonMapper;

@RestController
@RequestMapping("/api")
public class WordController {

  private final JsonMapper jsonMapper;

  public WordController(JsonMapper jsonMapper) {
    this.jsonMapper = jsonMapper;
  }

  /**
   * Liveness probe for Render. Returns 200 unconditionally (it must not depend on dictionary
   * contents, which are empty in the committed sample DB).
   */
  @GetMapping("/health")
  public ResponseEntity<String> health() {
    return ResponseEntity.ok().contentType(MediaType.APPLICATION_JSON).body("{\"status\":\"ok\"}");
  }

  /**
   * Look up a word in the dictionary. Returns 200 with full word JSON on hit, 404 with a suggestion
   * on miss, or 400 with an error for invalid input.
   */
  @GetMapping("/word/{word}")
  public ResponseEntity<String> lookup(@PathVariable String word) {
    String json = WordEngine.lookup(word);
    int status = statusFor(json);
    return ResponseEntity.status(status).contentType(MediaType.APPLICATION_JSON).body(json);
  }

  /** Spelling suggestions for a word. Returns JSON array. */
  @GetMapping("/suggest/{word}")
  public ResponseEntity<String> suggest(@PathVariable String word) {
    return ResponseEntity.ok()
        .contentType(MediaType.APPLICATION_JSON)
        .body(WordEngine.suggest(word));
  }

  /** Synonym suggestions for a word. Returns JSON array. */
  @GetMapping("/synonym/{word}")
  public ResponseEntity<String> synonym(@PathVariable String word) {
    return ResponseEntity.ok()
        .contentType(MediaType.APPLICATION_JSON)
        .body(WordEngine.synonym(word));
  }

  /**
   * Word of the day: the same word for every user on a given calendar day, rolling over at
   * midnight. Mirrors a normal lookup payload.
   */
  @GetMapping("/word-of-the-day")
  public ResponseEntity<String> wordOfTheDay() {
    long day = LocalDate.now().toEpochDay();
    String json = WordEngine.wordOfTheDay(day);
    return ResponseEntity.status(wotdStatusFor(json))
        .contentType(MediaType.APPLICATION_JSON)
        .body(json);
  }

  /**
   * Autocomplete for a prefix. Passes the user's history and suggested words as query params so the
   * engine can prioritize familiar completions.
   */
  @GetMapping("/autofill/{word}")
  public ResponseEntity<String> autofill(
      @PathVariable String word,
      @RequestParam(defaultValue = "[]") String history,
      @RequestParam(defaultValue = "[]") String suggested) {
    return ResponseEntity.ok()
        .contentType(MediaType.APPLICATION_JSON)
        .body(WordEngine.autofill(word, history, suggested));
  }

  /**
   * Derive the HTTP status from the engine's JSON payload instead of string-matching serialized
   * output: {"found": false} → 404, {"error": ...} → 400, otherwise 200.
   */
  private int statusFor(String json) {
    try {
      JsonNode node = jsonMapper.readTree(json);
      if (node.has("found") && !node.get("found").asBoolean()) {
        return HttpStatus.NOT_FOUND.value();
      }
      if (node.has("error")) {
        return HttpStatus.BAD_REQUEST.value();
      }
      return HttpStatus.OK.value();
    } catch (Exception e) {
      return HttpStatus.INTERNAL_SERVER_ERROR.value();
    }
  }

  /**
   * Status mapping for the word of the day: unlike a lookup, an empty dictionary is a server-side
   * condition, so the engine's {"error": ...} payload becomes 500 instead of statusFor's 400.
   */
  private int wotdStatusFor(String json) {
    try {
      JsonNode node = jsonMapper.readTree(json);
      return node.has("error") ? HttpStatus.INTERNAL_SERVER_ERROR.value() : HttpStatus.OK.value();
    } catch (Exception e) {
      return HttpStatus.INTERNAL_SERVER_ERROR.value();
    }
  }
}
