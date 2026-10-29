package com.quickquill.studio.controller;

import com.quickquill.studio.config.AuthToken;
import com.quickquill.studio.model.Session;
import com.quickquill.studio.model.User;
import com.quickquill.studio.service.AuthService;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import java.util.Map;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

@RestController
@RequestMapping("/api/auth")
public class AuthController {

  public record SignupRequest(
      @NotBlank String email, @NotBlank String password, @NotBlank String displayName) {
    @Override
    public String toString() {
      return "SignupRequest[REDACTED]";
    }
  }

  public record LoginRequest(@NotBlank String email, @NotBlank String password) {
    @Override
    public String toString() {
      return "LoginRequest[REDACTED]";
    }
  }

  public record ChangePasswordRequest(@NotBlank String oldPassword, @NotBlank String newPassword) {
    @Override
    public String toString() {
      return "ChangePasswordRequest[REDACTED]";
    }
  }

  private final AuthService authService;

  public AuthController(AuthService authService) {
    this.authService = authService;
  }

  /** Registers a user and returns a session token so the client can skip a second login call. */
  @PostMapping(value = "/signup", consumes = "application/json")
  // IllegalArgumentException (duplicate email) → GlobalExceptionHandler → 409
  public ResponseEntity<?> signup(@Valid @RequestBody SignupRequest request) {
    Session session =
        authService.signup(request.email(), request.password(), request.displayName());
    return ResponseEntity.status(HttpStatus.CREATED)
        .body(sessionResponse(session, "User registered successfully."));
  }

  @PostMapping(value = "/login", consumes = "application/json")
  // IllegalArgumentException (bad credentials) → GlobalExceptionHandler → 401
  public ResponseEntity<?> login(@Valid @RequestBody LoginRequest request) {
    Session session = authService.login(request.email(), request.password());
    return ResponseEntity.ok(sessionResponse(session, "User logged in successfully."));
  }

  @PostMapping("/logout")
  public ResponseEntity<?> logout(@RequestHeader("Authorization") String authorization) {
    authService.logout(AuthToken.fromHeader(authorization));
    return ResponseEntity.ok(Map.of("message", "User logged out successfully."));
  }

  /** Extends the session's expiry. Called by the client on app start to keep sessions alive. */
  @PostMapping("/refresh")
  // IllegalArgumentException (invalid/expired token) → GlobalExceptionHandler → 401
  public ResponseEntity<?> refresh(@RequestHeader("Authorization") String authorization) {
    Session session = authService.refreshSession(AuthToken.fromHeader(authorization));
    return ResponseEntity.ok(sessionResponse(session, "Session refreshed."));
  }

  @PostMapping(value = "/change-password", consumes = "application/json")
  // IllegalArgumentException (wrong password / invalid token) → GlobalExceptionHandler → 401
  public ResponseEntity<?> changePassword(
      @RequestHeader("Authorization") String authorization,
      @Valid @RequestBody ChangePasswordRequest request) {
    authService.changePassword(
        AuthToken.fromHeader(authorization), request.oldPassword(), request.newPassword());
    return ResponseEntity.ok(Map.of("message", "Password changed successfully."));
  }

  @PostMapping("/delete-account")
  // IllegalArgumentException (invalid token) → GlobalExceptionHandler → 401
  public ResponseEntity<?> deleteAccount(@RequestHeader("Authorization") String authorization) {
    authService.deleteAccount(AuthToken.fromHeader(authorization));
    return ResponseEntity.ok(Map.of("message", "Account deleted successfully."));
  }

  @PostMapping("/update")
  // IllegalArgumentException (email taken) → GlobalExceptionHandler → 409
  public ResponseEntity<?> update(
      @RequestHeader("Authorization") String authorization,
      @RequestParam String displayName,
      @RequestParam String email) {
    User user = authService.updateProfile(AuthToken.fromHeader(authorization), displayName, email);
    return ResponseEntity.ok(userJson(user));
  }

  @GetMapping("/me")
  // IllegalArgumentException (invalid/expired token) → GlobalExceptionHandler → 401
  public ResponseEntity<?> me(@RequestHeader("Authorization") String authorization) {
    User user = authService.validateSession(AuthToken.fromHeader(authorization));
    return ResponseEntity.ok(userJson(user));
  }

  private static Map<String, Object> userJson(User user) {
    return Map.of(
        "id", user.getId(),
        "email", user.getEmail(),
        "displayName", user.getDisplayName());
  }

  private static Map<String, Object> sessionResponse(Session session, String message) {
    return Map.of(
        "token", session.getToken(),
        "user", userJson(session.getUser()),
        "message", message);
  }
}
