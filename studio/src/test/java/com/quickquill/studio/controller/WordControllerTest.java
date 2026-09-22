package com.quickquill.studio.controller;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.content;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.jsonPath;
import static org.springframework.test.web.servlet.result.MockMvcResultMatchers.status;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Nested;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.webmvc.test.autoconfigure.AutoConfigureMockMvc;
import org.springframework.test.web.servlet.MockMvc;

@SpringBootTest
@AutoConfigureMockMvc
public class WordControllerTest {

  @Autowired private MockMvc mockMvc;

  @Nested
  @DisplayName("GET /api/word/{word}")
  class Lookup {

    @Test
    void shouldReturnWordForHit() throws Exception {
      mockMvc
          .perform(get("/api/word/hello"))
          .andExpect(status().isOk())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$.lemma").value("hello"))
          .andExpect(jsonPath("$.display_lemma").value("hello"))
          .andExpect(jsonPath("$.id").isNumber())
          .andExpect(jsonPath("$.senses[0].pos").value("interjection"))
          .andExpect(jsonPath("$.senses[0].definition").value("used as a greeting"))
          .andExpect(jsonPath("$.senses[0].synonyms[0]").value("hi"));
    }

    @Test
    void shouldReturn404WithoutSuggestionForUnknownWord() throws Exception {
      mockMvc
          .perform(get("/api/word/qzwxvqx"))
          .andExpect(status().isNotFound())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$.query").value("qzwxvqx"))
          .andExpect(jsonPath("$.found").value(false))
          // an unknown word must never be suggested as its own correction
          .andExpect(jsonPath("$.suggestion").doesNotExist());
    }

    @Test
    void shouldReturn400ForInvalidInput() throws Exception {
      mockMvc
          .perform(get("/api/word/%21%21"))
          .andExpect(status().isBadRequest())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$.error").value("Enter a valid word"));
    }
  }

  @Nested
  @DisplayName("GET /api/suggest/{word}")
  class Suggest {

    @Test
    void shouldReturnSimilarWords() throws Exception {
      mockMvc
          .perform(get("/api/suggest/helloo"))
          .andExpect(status().isOk())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$.length()").value(1))
          .andExpect(jsonPath("$[0]").value("hello"));
    }
  }

  @Nested
  @DisplayName("GET /api/synonym/{word}")
  class Synonym {

    @Test
    void shouldReturnJsonArray() throws Exception {
      mockMvc
          .perform(get("/api/synonym/hello"))
          .andExpect(status().isOk())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$").isArray());
    }
  }

  @Nested
  @DisplayName("GET /api/autofill/{word}")
  class Autofill {

    @Test
    void shouldReturnBestCompletion() throws Exception {
      mockMvc
          .perform(get("/api/autofill/he"))
          .andExpect(status().isOk())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$.completion").value("hello"));
    }

    @Test
    void shouldPreferWordFromHistory() throws Exception {
      mockMvc
          .perform(get("/api/autofill/he").param("history", "[\"help\"]"))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.completion").value("help"));
    }

    @Test
    void shouldReturnEmptyCompletionForNoMatch() throws Exception {
      mockMvc
          .perform(get("/api/autofill/xyz"))
          .andExpect(status().isOk())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$.completion").value(""));
    }
  }

  @Nested
  @DisplayName("GET /api/word-of-the-day")
  class WordOfTheDay {

    @Test
    void shouldReturnADictionaryWord() throws Exception {
      mockMvc
          .perform(get("/api/word-of-the-day"))
          .andExpect(status().isOk())
          .andExpect(content().contentTypeCompatibleWith("application/json"))
          .andExpect(jsonPath("$.lemma").isNotEmpty())
          .andExpect(jsonPath("$.display_lemma").isNotEmpty())
          .andExpect(jsonPath("$.query").isNotEmpty())
          .andExpect(jsonPath("$.senses").isArray());
    }
  }
}
