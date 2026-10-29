package com.quickquill.studio.controller;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.delete;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import com.quickquill.studio.model.SearchHistory;
import com.quickquill.studio.model.User;
import com.quickquill.studio.repository.SearchHistoryRepository;
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
public class SearchHistoryControllerTest {

  @Autowired private MockMvc mockMvc;

  @Autowired private SearchHistoryRepository searchHistoryRepository;

  @Autowired private AuthService authService;

  // Signup and login helpers needed because search-history is user specific

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

  private void record(String token, String word) throws Exception {
    mockMvc
        .perform(
            post("/api/search-history")
                .header("Authorization", "Bearer " + token)
                .param("word", word))
        .andExpect(status().isOk())
        .andExpect(jsonPath("$.message").value("Search recorded."));
  }

  @Nested
  @DisplayName("GET /api/search-history")
  class GetHistory {

    @Test
    void shouldReturnEmptyHistoryForFreshUser() throws Exception {
      signup("history@test.com", "pass123");
      String token = loginAndExtractToken("history@test.com", "pass123");

      mockMvc
          .perform(get("/api/search-history").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void shouldReturnWordsMostRecentFirst() throws Exception {
      signup("order@test.com", "pass123");
      String token = loginAndExtractToken("order@test.com", "pass123");

      record(token, "apple");
      Thread.sleep(10);
      record(token, "banana");
      Thread.sleep(10);
      record(token, "cherry");

      mockMvc
          .perform(get("/api/search-history").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(3))
          .andExpect(jsonPath("$[0]").value("cherry"))
          .andExpect(jsonPath("$[1]").value("banana"))
          .andExpect(jsonPath("$[2]").value("apple"));
    }

    @Test
    void shouldDeduplicateOnReRecord() throws Exception {
      signup("dedup@test.com", "pass123");
      String token = loginAndExtractToken("dedup@test.com", "pass123");

      record(token, "apple");
      Thread.sleep(10);
      record(token, "banana");
      Thread.sleep(10);
      record(token, "apple");

      mockMvc
          .perform(get("/api/search-history").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(2))
          .andExpect(jsonPath("$[0]").value("apple"))
          .andExpect(jsonPath("$[1]").value("banana"));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(get("/api/search-history").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingToken() throws Exception {
      mockMvc.perform(get("/api/search-history")).andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("POST /api/search-history")
  class Record {

    @Test
    void shouldRecordSearch() throws Exception {
      signup("rec@test.com", "pass123");
      String token = loginAndExtractToken("rec@test.com", "pass123");

      record(token, "hello");

      mockMvc
          .perform(get("/api/search-history").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(1))
          .andExpect(jsonPath("$[0]").value("hello"));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(
              post("/api/search-history")
                  .header("Authorization", "Bearer bogus-token")
                  .param("word", "hello"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingBody() throws Exception {
      mockMvc.perform(post("/api/search-history")).andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("DELETE /api/search-history")
  class Clear {

    @Test
    void shouldClearHistory() throws Exception {
      signup("clear@test.com", "pass123");
      String token = loginAndExtractToken("clear@test.com", "pass123");

      record(token, "apple");
      record(token, "banana");

      mockMvc
          .perform(delete("/api/search-history").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.message").value("Search history cleared."));

      mockMvc
          .perform(get("/api/search-history").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(delete("/api/search-history").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingToken() throws Exception {
      mockMvc.perform(delete("/api/search-history")).andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("history size cap")
  class SizeCap {

    @Test
    void shouldKeepAtMost500Words() throws Exception {
      signup("cap@test.com", "pass123");
      String token = loginAndExtractToken("cap@test.com", "pass123");
      User user = authService.validateSession(token);

      for (int i = 0; i < 505; i++) {
        searchHistoryRepository.save(new SearchHistory(user, "word-" + i));
      }
      record(token, "final-word");

      mockMvc
          .perform(get("/api/search-history").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(500));
    }
  }
}
