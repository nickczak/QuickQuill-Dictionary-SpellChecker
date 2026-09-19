package com.quickquill.studio.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.servlet.config.annotation.CorsRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;

/**
 * Allows the GitHub Pages frontend (a different origin than the Render backend) to call the REST
 * API. Without these headers the browser rejects every cross-origin /api request.
 */
@Configuration
public class CorsConfig {

  @Bean
  public WebMvcConfigurer corsConfigurer(
      @Value("${quickquill.cors.allowed-origins:https://nickczak.github.io}")
          String allowedOrigins) {
    return new WebMvcConfigurer() {
      @Override
      public void addCorsMappings(CorsRegistry registry) {
        registry
            .addMapping("/api/**")
            .allowedOrigins(allowedOrigins.split(","))
            .allowedMethods("GET", "POST", "PUT", "DELETE", "OPTIONS")
            .allowedHeaders("*")
            .allowCredentials(true);
      }
    };
  }
}
