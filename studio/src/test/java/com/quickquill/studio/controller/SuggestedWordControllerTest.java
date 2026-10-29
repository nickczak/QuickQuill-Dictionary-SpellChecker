package com.quickquill.studio.controller;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.delete;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import com.quickquill.studio.model.SuggestedWord;
import com.quickquill.studio.model.User;
import com.quickquill.studio.repository.SuggestedWordRepository;
import com.quickquill.studio.service.AuthService;
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
public class SuggestedWordControllerTest {

  @Autowired private MockMvc mockMvc;

  @Autowired private SuggestedWordRepository suggestedWordRepository;

  @Autowired private AuthService authService;

  private void signup(String email, String password) throws Exception {
    mockMvc
        .perform(
            post("/api/auth/signup")
                .contentType(MediaType.APPLICATION_JSON)
                .content(
                    JsonMapper.builder()
                        .build()
                        .writeValueAsString(
                            Map.of("email", email, "password", password, "displayName", email))))
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
    return response.split("\"token\":\"")[1].split("\"")[0];
  }

  private void sync(String token, String... words) throws Exception {
    var request = post("/api/suggested-words/sync").header("Authorization", "Bearer " + token);
    for (String word : words) {
      request.param("word", word);
    }
    mockMvc
        .perform(request)
        .andExpect(status().isOk())
        .andExpect(jsonPath("$.message").value("Suggested words synced."));
  }

  @Nested
  @DisplayName("GET /api/suggested-words")
  class GetWords {

    @Test
    void shouldReturnEmptyListForFreshUser() throws Exception {
      signup("sugg@test.com", "pass123");
      String token = loginAndExtractToken("sugg@test.com", "pass123");

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void shouldReturnWordsMostRecentFirst() throws Exception {
      signup("order2@test.com", "pass123");
      String token = loginAndExtractToken("order2@test.com", "pass123");

      sync(token, "alpha");
      Thread.sleep(10);
      sync(token, "beta");
      Thread.sleep(10);
      sync(token, "gamma");

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(3))
          .andExpect(jsonPath("$[0]").value("gamma"))
          .andExpect(jsonPath("$[1]").value("beta"))
          .andExpect(jsonPath("$[2]").value("alpha"));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingToken() throws Exception {
      mockMvc.perform(get("/api/suggested-words")).andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("POST /api/suggested-words/sync")
  class Sync {

    @Test
    void shouldRecordMultipleWords() throws Exception {
      signup("sync@test.com", "pass123");
      String token = loginAndExtractToken("sync@test.com", "pass123");

      sync(token, "apple", "banana", "cherry");

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(3));
    }

    @Test
    void shouldDeduplicateWordsOnResync() throws Exception {
      signup("dedup2@test.com", "pass123");
      String token = loginAndExtractToken("dedup2@test.com", "pass123");

      sync(token, "apple", "apple");

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(1))
          .andExpect(jsonPath("$[0]").value("apple"));
    }

    @Test
    void shouldDoNothingWhenNoWordsProvided() throws Exception {
      signup("nosync@test.com", "pass123");
      String token = loginAndExtractToken("nosync@test.com", "pass123");

      sync(token);

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void shouldIgnoreBlankWords() throws Exception {
      signup("blank2@test.com", "pass123");
      String token = loginAndExtractToken("blank2@test.com", "pass123");

      sync(token, "apple", "", "  ");

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(1))
          .andExpect(jsonPath("$[0]").value("apple"));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(
              post("/api/suggested-words/sync")
                  .header("Authorization", "Bearer bogus-token")
                  .param("word", "x"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingToken() throws Exception {
      mockMvc
          .perform(post("/api/suggested-words/sync").param("word", "x"))
          .andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("DELETE /api/suggested-words")
  class Clear {

    @Test
    void shouldClearWords() throws Exception {
      signup("clr2@test.com", "pass123");
      String token = loginAndExtractToken("clr2@test.com", "pass123");

      sync(token, "apple", "banana");

      mockMvc
          .perform(delete("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.message").value("Suggested words cleared."));

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(delete("/api/suggested-words").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingToken() throws Exception {
      mockMvc.perform(delete("/api/suggested-words")).andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("suggested words size cap")
  class SizeCap {

    @Test
    void shouldKeepAtMost1000Words() throws Exception {
      signup("cap2@test.com", "pass123");
      String token = loginAndExtractToken("cap2@test.com", "pass123");
      User user = authService.validateSession(token);

      for (int i = 0; i < 1000; i++) {
        suggestedWordRepository.save(new SuggestedWord(user, "word-" + i));
      }
      sync(token, "final-word");

      mockMvc
          .perform(get("/api/suggested-words").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(1000));
    }
  }
}
