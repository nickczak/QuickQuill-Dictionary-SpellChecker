package com.quickquill.studio.controller;

import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.delete;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.get;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.post;
import static org.springframework.test.web.servlet.request.MockMvcRequestBuilders.put;
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
public class DocumentControllerTest {

  @Autowired private MockMvc mockMvc;

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

  /** Creates a document via the API and returns its id as a long. */
  private long createDocument(String token, String title) throws Exception {
    String response =
        mockMvc
            .perform(
                post("/api/documents")
                    .header("Authorization", "Bearer " + token)
                    .param("title", title))
            .andExpect(status().isOk())
            .andReturn()
            .getResponse()
            .getContentAsString();
    return Long.parseLong(response.split("\"id\":")[1].split(",")[0]);
  }

  @Nested
  @DisplayName("GET /api/documents")
  class ListDocuments {

    @Test
    void shouldReturnEmptyListForNewUser() throws Exception {
      signup("docs1@test.com", "pass123");
      String token = loginAndExtractToken("docs1@test.com", "pass123");

      mockMvc
          .perform(get("/api/documents").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void shouldListCreatedDocuments() throws Exception {
      signup("docs2@test.com", "pass123");
      String token = loginAndExtractToken("docs2@test.com", "pass123");
      createDocument(token, "First");
      createDocument(token, "Second");

      mockMvc
          .perform(get("/api/documents").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.length()").value(2))
          .andExpect(jsonPath("$[0].id").isNumber())
          .andExpect(jsonPath("$[0].title").exists())
          .andExpect(jsonPath("$[0].updatedAt").exists())
          // Summaries never leak content.
          .andExpect(jsonPath("$[0].content").doesNotExist());
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(get("/api/documents").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingToken() throws Exception {
      mockMvc.perform(get("/api/documents")).andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("POST /api/documents")
  class CreateDocument {

    @Test
    void shouldCreateDocumentWithGivenTitle() throws Exception {
      signup("docs3@test.com", "pass123");
      String token = loginAndExtractToken("docs3@test.com", "pass123");

      mockMvc
          .perform(
              post("/api/documents")
                  .header("Authorization", "Bearer " + token)
                  .param("title", "My Story"))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.id").isNumber())
          .andExpect(jsonPath("$.title").value("My Story"))
          .andExpect(jsonPath("$.content").value(""))
          .andExpect(jsonPath("$.updatedAt").exists());

      mockMvc
          .perform(get("/api/documents").header("Authorization", "Bearer " + token))
          .andExpect(jsonPath("$.length()").value(1))
          .andExpect(jsonPath("$[0].title").value("My Story"));
    }

    @Test
    void shouldDefaultTitleToUntitledWhenMissing() throws Exception {
      signup("docs4@test.com", "pass123");
      String token = loginAndExtractToken("docs4@test.com", "pass123");

      mockMvc
          .perform(post("/api/documents").header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.title").value("Untitled"));
    }

    @Test
    void shouldDefaultTitleToUntitledWhenBlank() throws Exception {
      signup("docs5@test.com", "pass123");
      String token = loginAndExtractToken("docs5@test.com", "pass123");

      mockMvc
          .perform(
              post("/api/documents")
                  .header("Authorization", "Bearer " + token)
                  .param("title", "   "))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.title").value("Untitled"));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(post("/api/documents").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }
  }

  @Nested
  @DisplayName("GET /api/documents/{id}")
  class GetDocument {

    @Test
    void shouldReturnFullDocumentWithContent() throws Exception {
      signup("docs6@test.com", "pass123");
      String token = loginAndExtractToken("docs6@test.com", "pass123");
      long id = createDocument(token, "Notes");

      mockMvc.perform(
          put("/api/documents/" + id)
              .header("Authorization", "Bearer " + token)
              .param("content", "abc"));

      mockMvc
          .perform(get("/api/documents/" + id).header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.id").value(id))
          .andExpect(jsonPath("$.title").value("Notes"))
          .andExpect(jsonPath("$.content").value("abc"))
          .andExpect(jsonPath("$.updatedAt").exists());
    }

    @Test
    void shouldReturn404ForUnknownId() throws Exception {
      signup("docs7@test.com", "pass123");
      String token = loginAndExtractToken("docs7@test.com", "pass123");

      mockMvc
          .perform(get("/api/documents/999999").header("Authorization", "Bearer " + token))
          .andExpect(status().isNotFound());
    }

    @Test
    void shouldReturn404ForAnotherUsersDocument() throws Exception {
      signup("docs8@test.com", "pass123");
      String ownerToken = loginAndExtractToken("docs8@test.com", "pass123");
      long id = createDocument(ownerToken, "Private");

      signup("docs9@test.com", "pass123");
      String otherToken = loginAndExtractToken("docs9@test.com", "pass123");

      mockMvc
          .perform(get("/api/documents/" + id).header("Authorization", "Bearer " + otherToken))
          .andExpect(status().isNotFound());
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(get("/api/documents/1").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }
  }

  @Nested
  @DisplayName("PUT /api/documents/{id}")
  class SaveDocument {

    @Test
    void shouldSaveContent() throws Exception {
      signup("docs10@test.com", "pass123");
      String token = loginAndExtractToken("docs10@test.com", "pass123");
      long id = createDocument(token, "Draft");

      mockMvc
          .perform(
              put("/api/documents/" + id)
                  .header("Authorization", "Bearer " + token)
                  .param("content", "hello doc"))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.id").value(id))
          .andExpect(jsonPath("$.content").value("hello doc"));

      mockMvc
          .perform(get("/api/documents/" + id).header("Authorization", "Bearer " + token))
          .andExpect(jsonPath("$.content").value("hello doc"));
    }

    @Test
    void shouldOverwriteExistingContent() throws Exception {
      signup("docs11@test.com", "pass123");
      String token = loginAndExtractToken("docs11@test.com", "pass123");
      long id = createDocument(token, "Draft");

      mockMvc.perform(
          put("/api/documents/" + id)
              .header("Authorization", "Bearer " + token)
              .param("content", "first"));
      mockMvc.perform(
          put("/api/documents/" + id)
              .header("Authorization", "Bearer " + token)
              .param("content", "second"));

      mockMvc
          .perform(get("/api/documents/" + id).header("Authorization", "Bearer " + token))
          .andExpect(jsonPath("$.content").value("second"));
    }

    @Test
    void shouldReturn404ForAnotherUsersDocument() throws Exception {
      signup("docs12@test.com", "pass123");
      String ownerToken = loginAndExtractToken("docs12@test.com", "pass123");
      long id = createDocument(ownerToken, "Private");

      signup("docs13@test.com", "pass123");
      String otherToken = loginAndExtractToken("docs13@test.com", "pass123");

      mockMvc
          .perform(
              put("/api/documents/" + id)
                  .header("Authorization", "Bearer " + otherToken)
                  .param("content", "x"))
          .andExpect(status().isNotFound());
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(
              put("/api/documents/1")
                  .header("Authorization", "Bearer bogus-token")
                  .param("content", "x"))
          .andExpect(status().isUnauthorized());
    }

    @Test
    void shouldReturn400ForMissingContent() throws Exception {
      signup("docs14@test.com", "pass123");
      String token = loginAndExtractToken("docs14@test.com", "pass123");
      long id = createDocument(token, "Draft");

      mockMvc
          .perform(put("/api/documents/" + id).header("Authorization", "Bearer " + token))
          .andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("POST /api/documents/{id}/rename")
  class RenameDocument {

    @Test
    void shouldRenameDocument() throws Exception {
      signup("docs15@test.com", "pass123");
      String token = loginAndExtractToken("docs15@test.com", "pass123");
      long id = createDocument(token, "Old Name");

      mockMvc
          .perform(
              post("/api/documents/" + id + "/rename")
                  .header("Authorization", "Bearer " + token)
                  .param("title", "New Name"))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.id").value(id))
          .andExpect(jsonPath("$.title").value("New Name"));

      mockMvc
          .perform(get("/api/documents/" + id).header("Authorization", "Bearer " + token))
          .andExpect(jsonPath("$.title").value("New Name"));
    }

    @Test
    void shouldReturn404ForAnotherUsersDocument() throws Exception {
      signup("docs16@test.com", "pass123");
      String ownerToken = loginAndExtractToken("docs16@test.com", "pass123");
      long id = createDocument(ownerToken, "Private");

      signup("docs17@test.com", "pass123");
      String otherToken = loginAndExtractToken("docs17@test.com", "pass123");

      mockMvc
          .perform(
              post("/api/documents/" + id + "/rename")
                  .header("Authorization", "Bearer " + otherToken)
                  .param("title", "Hacked"))
          .andExpect(status().isNotFound());
    }

    @Test
    void shouldReturn400ForMissingTitle() throws Exception {
      signup("docs18@test.com", "pass123");
      String token = loginAndExtractToken("docs18@test.com", "pass123");
      long id = createDocument(token, "Draft");

      mockMvc
          .perform(
              post("/api/documents/" + id + "/rename").header("Authorization", "Bearer " + token))
          .andExpect(status().isBadRequest());
    }
  }

  @Nested
  @DisplayName("DELETE /api/documents/{id}")
  class DeleteDocument {

    @Test
    void shouldDeleteDocument() throws Exception {
      signup("docs19@test.com", "pass123");
      String token = loginAndExtractToken("docs19@test.com", "pass123");
      long id = createDocument(token, "Doomed");

      mockMvc
          .perform(delete("/api/documents/" + id).header("Authorization", "Bearer " + token))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.message").value("Document deleted."));

      mockMvc
          .perform(get("/api/documents/" + id).header("Authorization", "Bearer " + token))
          .andExpect(status().isNotFound());

      mockMvc
          .perform(get("/api/documents").header("Authorization", "Bearer " + token))
          .andExpect(jsonPath("$.length()").value(0));
    }

    @Test
    void shouldReturn404ForAnotherUsersDocument() throws Exception {
      signup("docs20@test.com", "pass123");
      String ownerToken = loginAndExtractToken("docs20@test.com", "pass123");
      long id = createDocument(ownerToken, "Keep me");

      signup("docs21@test.com", "pass123");
      String otherToken = loginAndExtractToken("docs21@test.com", "pass123");

      mockMvc
          .perform(delete("/api/documents/" + id).header("Authorization", "Bearer " + otherToken))
          .andExpect(status().isNotFound());

      // The owner's document survives the foreign delete attempt.
      mockMvc
          .perform(get("/api/documents/" + id).header("Authorization", "Bearer " + ownerToken))
          .andExpect(status().isOk())
          .andExpect(jsonPath("$.title").value("Keep me"));
    }

    @Test
    void shouldReturn401ForInvalidToken() throws Exception {
      mockMvc
          .perform(delete("/api/documents/1").header("Authorization", "Bearer bogus-token"))
          .andExpect(status().isUnauthorized());
    }
  }
}
