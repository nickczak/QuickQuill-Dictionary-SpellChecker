package com.quickquill.studio.config;

import java.util.Map;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.http.converter.HttpMessageNotReadableException;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

@RestControllerAdvice
public class GlobalExceptionHandler {

  // Framework exception messages can include rejected credentials or raw JSON.
  // Handle them without logging the exception or reflecting its values to the client.
  @ExceptionHandler({MethodArgumentNotValidException.class, HttpMessageNotReadableException.class})
  public ResponseEntity<Map<String, String>> handleInvalidRequest() {
    return ResponseEntity.badRequest().body(Map.of("error", "Invalid request body."));
  }

  @ExceptionHandler(IllegalArgumentException.class)
  public ResponseEntity<Map<String, String>> handleIllegalArgument(IllegalArgumentException e) {
    String msg = e.getMessage();
    HttpStatus status = HttpStatus.BAD_REQUEST;
    if (msg != null) {
      if (msg.contains("registered")) {
        status = HttpStatus.CONFLICT;
      } else if (msg.contains("password")
          || msg.contains("session")
          || msg.contains("login")
          || msg.contains("token")) {
        status = HttpStatus.UNAUTHORIZED;
      }
    }
    return ResponseEntity.status(status).body(Map.of("error", msg));
  }
}
