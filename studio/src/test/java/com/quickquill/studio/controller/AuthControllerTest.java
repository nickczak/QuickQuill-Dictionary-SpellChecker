package com.quickquill.studio.controller;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import java.util.Map;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.http.MediaType;
import org.springframework.test.web.servlet.MockMvc;
import org.springframework.transaction.annotation.Transactional;
import tools.jackson.databind.json.JsonMapper;

@SpringBootTest
@AutoConfigureMockMvc
@Transactional
public class AuthControllerTest {

  @Autowired private MockMvc mockMvc;

  private void signup(String email, String password, String displayName) throws Exception {
    mockMvc
        .perform(
            post("/api/auth/signup")
                .contentType(MediaType.APPLICATION_JSON)
                .content(
                    JsonMapper.builder()
                        .build()
                        .writeValueAsString(
                            Map.of(
                                "email", email, "password", password, "displayName", displayName))))
        .andExpect(status().isCreated());
  }

  private String loginAndExtractToken(String email, String password) throws Exception {
    String response =
        mockMvc
            .perform(
                post("/api/auth/login")
                    .contentType(MediaType.APPLICATION_JSON)
                    .content(
                        JsonMapper.builder()
                            .build()
                            .writeValueAsString(Map.of("email", email, "password", password))))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.token").isNotEmpty())
            .andReturn()
            .getResponse()
            .getContentAsString();
    // Extract token from JSON response
    return response.split("\"token\":\"")[1].split("\"")[0];
  }

  @Nested
  @DisplayName("POST /api/auth/signup")
  class Signup {

    @Test
    void shouldCreateNewUser() throws Exception {
      mockMvc
          .perform(
              post("/api/auth/signup")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of(
                                  "email",
                                  "test@test.com",
                                  "password",
                                  "pass123",
                                  "displayName",
                                  "Test User"))))
          .andExpect(status().isCreated())
          .andExpect(jsonPath("$.token").isNotEmpty())
          .andExpect(jsonPath("$.user.email").value("test@test.com"))
          .andExpect(jsonPath("$.user.displayName").value("Test User"))
          .andExpect(jsonPath("$.message").value("User registered successfully."));
    }

    @Test
    void shouldReturn409ForDuplicateEmail() throws Exception {
      signup("dup@test.com", "pass123", "First");

      mockMvc
          .perform(
              post("/api/auth/signup")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of(
                                  "email",
                                  "dup@test.com",
                                  "password",
                                  "pass456",
                                  "displayName",
                                  "Second"))))
          .andExpect(status().isConflict())
          .andExpect(jsonPath("$.error").value("Email already registered."));
    }

    @Test
    void shouldReturn400ForMissingBody() throws Exception {
      mockMvc
          .perform(post("/api/auth/signup").contentType(MediaType.APPLICATION_JSON))
          .andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("POST /api/auth/login")
  class Login {

    @Test
    void shouldReturnSession() throws Exception {
      signup("login@test.com", "pass123", "Login User");

      mockMvc
          .perform(
              post("/api/auth/login")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("email", "login@test.com", "password", "pass123"))))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.token").isNotEmpty())
          .andExpect(jsonPath("$.user.email").value("login@test.com"))
          .andExpect(jsonPath("$.user.displayName").value("Login User"))
          .andExpect(jsonPath("$.message").value("User logged in successfully."));
    }

    @Test
    void shouldReturn401ForWrongPassword() throws Exception {
      signup("wrong@test.com", "pass123", "Wrong User");

      mockMvc
          .perform(
              post("/api/auth/login")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("email", "wrong@test.com", "password", "bad"))))
          .andExpect(status().isUnauthorized())
          .andExpect(jsonPath("$.error").value("Invalid email or password."));
    }

    @Test
    void shouldReturn401ForNonexistentEmail() throws Exception {
      mockMvc
          .perform(
              post("/api/auth/login")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("email", "ghost@test.com", "password", "pass123"))))
          .andExpect(status().isUnauthorized())
          .andExpect(jsonPath("$.error").value("Invalid email or password."));
    }
  }

  @Nested
  @DisplayName("POST /api/auth/logout")
  class Logout {

    @Test
    void shouldLogoutSuccessfully() throws Exception {
      signup("logout@test.com", "pass123", "Logout User");
      String token = loginAndExtractToken("logout@test.com", "pass123");

      mockMvc
          .perform(post("/api/auth/logout").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.message").value("User logged out successfully."));
    }

    @Test
    void shouldHandleLogoutWithInvalidToken() throws Exception {
      mockMvc
          .perform(post("/api/auth/logout").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isOk());
    }
  }

  @Nested
  @DisplayName("POST /api/auth/refresh")
  class Refresh {

    @Test
    void shouldRefreshSession() throws Exception {
      signup("refresh@test.com", "pass123", "Refresh User");
      String token = loginAndExtractToken("refresh@test.com", "pass123");

      mockMvc
          .perform(post("/api/auth/refresh").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.token").isNotEmpty())
          .andExpect(jsonPath("$.message").value("Session refreshed."));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(post("/api/auth/refresh").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized())
          .andExpect(jsonPath("$.error").value("Invalid session."));
    }
  }

  @Nested
  @DisplayName("POST /api/auth/change-password")
  class ChangePassword {

    @Test
    void shouldChangePassword() throws Exception {
      signup("chg@test.com", "oldpass", "Chg User");

      mockMvc
          .perform(
              post("/api/auth/change-password")
                  .header(
                      "Authorization", "Bearer " + loginAndExtractToken("chg@test.com", "oldpass"))
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("oldPassword", "oldpass", "newPassword", "newpass"))))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.message").value("Password changed successfully."));

      // Old password should no longer work
      mockMvc
          .perform(
              post("/api/auth/login")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("email", "chg@test.com", "password", "oldpass"))))
          .andExpect(status().isUnauthorized());

      // New password should work
      mockMvc
          .perform(
              post("/api/auth/login")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("email", "chg@test.com", "password", "newpass"))))
          .andExpect(status().isOk());
    }

    @Test
    void shouldReturn401ForWrongOldPassword() throws Exception {
      signup("chg2@test.com", "realpass", "Chg2 User");

      mockMvc
          .perform(
              post("/api/auth/change-password")
                  .header(
                      "Authorization",
                      "Bearer " + loginAndExtractToken("chg2@test.com", "realpass"))
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("oldPassword", "wrongpass", "newPassword", "newpass"))))
          .andExpect(status().isUnauthorized())
          .andExpect(jsonPath("$.error").value("Wrong password."));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(
              post("/api/auth/change-password")
                  .header("Authorization", "Bearer bogus-token")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(Map.of("oldPassword", "old", "newPassword", "new"))))
          .andExpect(status().isUnauthorized());
    }
  }

  @Nested
  @DisplayName("POST /api/auth/delete-account")
  class DeleteAccount {

    @Test
    void shouldDeleteAccount() throws Exception {
      signup("del@test.com", "pass123", "Del User");

      mockMvc
          .perform(
              post("/api/auth/delete-account")
                  .header(
                      "Authorization", "Bearer " + loginAndExtractToken("del@test.com", "pass123")))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.message").value("Account deleted successfully."));

      // Account should no longer be loginable
      mockMvc
          .perform(
              post("/api/auth/login")
                  .contentType(MediaType.APPLICATION_JSON)
                  .content(
                      JsonMapper.builder()
                          .build()
                          .writeValueAsString(
                              Map.of("email", "del@test.com", "password", "pass123"))))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(post("/api/auth/delete-account").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }
  }

  @Nested
  @DisplayName("GET /api/auth/me")
  class Me {

    @Test
    void shouldReturnCurrentUser() throws Exception {
      signup("me@test.com", "pass123", "Me User");

      mockMvc
          .perform(
              get("/api/auth/me")
                  .header(
                      "Authorization", "Bearer " + loginAndExtractToken("me@test.com", "pass123")))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.email").value("me@test.com"))
          .andExpect(jsonPath("$.displayName").value("Me User"))
          .andExpect(jsonPath("$.id").isNumber());
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(get("/api/auth/me").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn401AfterLogout() throws Exception {
      signup("me2@test.com", "pass123", "Me2 User");
      String token = loginAndExtractToken("me2@test.com", "pass123");

      mockMvc.perform(post("/api/auth/logout").header("Authorization", "Bearer " + token));

      mockMvc
          .perform(get("/api/auth/me").header("Authorization", "Bearer " + token))
          .andExpect(status().isUnauthorized());
    }
  }

  @Nested
  @DisplayName("POST /api/auth/update")
  class Update {

    @Test
    void shouldUpdateProfile() throws Exception {
      signup("upd@test.com", "pass123", "Old Name");

      mockMvc
          .perform(
              post("/api/auth/update")
                  .header(
                      "Authorization", "Bearer " + loginAndExtractToken("upd@test.com", "pass123"))
                  .param("displayName", "New Name")
                  .param("email", "upd@test.com"))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.displayName").value("New Name"))
          .andExpect(jsonPath("$.email").value("upd@test.com"));
    }

    @Test
    void shouldUpdateEmail() throws Exception {
      signup("upd2@test.com", "pass123", "Upd2 User");

      mockMvc
          .perform(
              post("/api/auth/update")
                  .header(
                      "Authorization", "Bearer " + loginAndExtractToken("upd2@test.com", "pass123"))
                  .param("displayName", "Upd2 User")
                  .param("email", "newupd2@test.com"))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.email").value("newupd2@test.com"));
    }

    @Test
    void shouldReturn409ForDuplicateEmail() throws Exception {
      signup("taken@test.com", "pass123", "Taken");
      signup("updater@test.com", "pass123", "Updater");

      mockMvc
          .perform(
              post("/api/auth/update")
                  .header(
                      "Authorization",
                      "Bearer " + loginAndExtractToken("updater@test.com", "pass123"))
                  .param("displayName", "Updater")
                  .param("email", "taken@test.com"))
          .andExpect(status().isConflict());
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(
              post("/api/auth/update")
                  .header("Authorization", "Bearer bogus-token")
                  .param("displayName", "X")
                  .param("email", "x@test.com"))
          .andExpect(status().isUnauthorized());
    }
  }
}
