package com.quickquill.studio.controller;

import static org.assertj.core.api.Assertions.assertThat;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import com.quickquill.studio.model.Session;
import com.quickquill.studio.model.User;
import com.quickquill.studio.service.AuthService;
import java.util.Map;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.system.CapturedOutput;
import org.springframework.boot.test.system.OutputCaptureExtension;
import org.springframework.boot.webmvc.test.autoconfigure.WebMvcTest;
import org.springframework.http.MediaType;
import org.springframework.test.context.bean.override.mockito.MockitoBean;
import org.springframework.test.web.servlet.MockMvc;
import tools.jackson.databind.json.JsonMapper;

@WebMvcTest(AuthController.class)
@ExtendWith(OutputCaptureExtension.class)
class AuthRequestTest {
  @Autowired private MockMvc mockMvc;
  @MockitoBean private AuthService authService;

  private static final String SECRET = "secret-quote-\"-ampersand-&-unicode-é";
  private final JsonMapper mapper = JsonMapper.builder().build();

  @ParameterizedTest
  @ValueSource(strings = {"signup", "login", "change-password"})
  void rejectsQueryCredentials(String endpoint) throws Exception {
    mockMvc
        .perform(
            post("/api/auth/" + endpoint)
                .contentType(MediaType.APPLICATION_JSON)
                .header("Authorization", "Bearer test-token")
                .queryParam("email", "me@test.com")
                .queryParam("password", SECRET)
                .queryParam("displayName", "Me")
                .queryParam("oldPassword", SECRET)
                .queryParam("newPassword", SECRET))
        .andExpect(status().isBadRequest());
    verifyNoInteractions(authService);
  }

  @ParameterizedTest
  @ValueSource(strings = {"signup", "login", "change-password"})
  void rejectsFormCredentials(String endpoint) throws Exception {
    mockMvc
        .perform(
            post("/api/auth/" + endpoint)
                .contentType(MediaType.APPLICATION_FORM_URLENCODED)
                .header("Authorization", "Bearer test-token")
                .content(
                    "email=me%40test.com&password=secret&displayName=Me&oldPassword=old&newPassword=new"))
        .andExpect(status().isUnsupportedMediaType());
    verifyNoInteractions(authService);
  }

  @ParameterizedTest
  @ValueSource(strings = {"signup", "login", "change-password"})
  void rejectsMissingNullAndBlankFields(String endpoint, CapturedOutput output) throws Exception {
    for (String body :
        new String[] {
          "{}",
          "null",
          "{\"email\":null,\"password\":null,\"displayName\":null,\"oldPassword\":null,\"newPassword\":null}",
          "{\"email\":\" \",\"password\":\" \",\"displayName\":\" \",\"oldPassword\":\""
              + " \",\"newPassword\":\" \"}",
          mapper.writeValueAsString(Map.of("password", SECRET, "oldPassword", SECRET))
        }) {
      mockMvc
          .perform(
              post("/api/auth/" + endpoint)
                  .contentType(MediaType.APPLICATION_JSON)
                  .header("Authorization", "Bearer test-token")
                  .content(body))
          .andExpect(status().isBadRequest())
          .andExpect(jsonPath("$.error").value("Invalid request body."));
    }
    verifyNoInteractions(authService);
    assertThat(output.getAll()).doesNotContain("secret-quote-");
  }

  @Test
  void malformedJsonDoesNotLeakCredentials(CapturedOutput output) throws Exception {
    mockMvc
        .perform(
            post("/api/auth/login")
                .contentType(MediaType.APPLICATION_JSON)
                .content("{\"password\":\"sensitive-marker\",\"email\":"))
        .andExpect(status().isBadRequest())
        .andExpect(jsonPath("$.error").value("Invalid request body."));
    verifyNoInteractions(authService);
    assertThat(output.getAll()).doesNotContain("sensitive-marker");
  }

  @ParameterizedTest
  @ValueSource(strings = {"signup", "login"})
  void acceptsJsonWithoutEchoingCredentials(String endpoint, CapturedOutput output)
      throws Exception {
    User user = new User("me@test.com", "stored-hash", "Me");
    org.springframework.test.util.ReflectionTestUtils.setField(user, "id", 1L);
    Session session = new Session(user);
    if (endpoint.equals("signup")) {
      when(authService.signup("me@test.com", SECRET, "Me")).thenReturn(session);
    } else {
      when(authService.login("me@test.com", SECRET)).thenReturn(session);
    }
    String response =
        mockMvc
            .perform(
                post("/api/auth/" + endpoint)
                    .contentType(MediaType.APPLICATION_JSON)
                    .content(
                        mapper.writeValueAsString(
                            Map.of(
                                "email", "me@test.com", "password", SECRET, "displayName", "Me"))))
            .andExpect(status().is(endpoint.equals("signup") ? 201 : 200))
            .andExpect(jsonPath("$.token").isNotEmpty())
            .andExpect(jsonPath("$.user.password").doesNotExist())
            .andReturn()
            .getResponse()
            .getContentAsString();
    assertThat(response).doesNotContain("secret-quote-", "stored-hash");
    assertThat(output.getAll()).doesNotContain("secret-quote-", "stored-hash");
  }

  @Test
  void changesPasswordUsingJsonAndBearerHeader() throws Exception {
    mockMvc
        .perform(
            post("/api/auth/change-password")
                .contentType(MediaType.APPLICATION_JSON)
                .header("Authorization", "Bearer test-token")
                .content(
                    mapper.writeValueAsString(
                        Map.of("oldPassword", SECRET, "newPassword", "new-secret"))))
        .andExpect(status().isOk());
    verify(authService).changePassword("test-token", SECRET, "new-secret");
  }

  @Test
  void requestDescriptionsRedactSensitiveValues() {
    assertThat(new AuthController.SignupRequest("me@test.com", SECRET, "Me").toString())
        .isEqualTo("SignupRequest[REDACTED]");
    assertThat(new AuthController.LoginRequest("me@test.com", SECRET).toString())
        .isEqualTo("LoginRequest[REDACTED]");
    assertThat(new AuthController.ChangePasswordRequest(SECRET, SECRET).toString())
        .isEqualTo("ChangePasswordRequest[REDACTED]");
  }
}
