import { Injectable, inject } from '@angular/core';
import { HttpClient, HttpResponse, HttpParams } from '@angular/common/http';
import { Observable, timeout } from 'rxjs';
import { WordResponse, WordNotFound, WordError, AutofillResponse } from '../models/word.models';
import { AuthUser, AuthResponse } from '../models/auth.models';
import { DocumentResponse, DocumentSummary } from '../models/document.models';
import { environment } from '../../environments/environment';

/** Absolute API URL (empty in development, where the dev proxy adds the /api prefix). */
const endpoint = (path: string) => environment.apiBaseUrl + path;

export function apiErrorMessage(err: unknown, fallback: string): string {
  // Prefer the server's own message (e.g. "Wrong password.", "Email already registered.").
  const server = (err as { error?: { error?: string } })?.error?.error;
  if (server) {
    return server;
  }
  const status = (err as { status?: number })?.status;
  if (status === 404) {
    return 'This feature is not available right now. Please try again later.';
  }
  if (status === 409) {
    return 'An account with this email already exists.';
  }
  if (status === 401) {
    return 'Invalid email or password.';
  }
  return fallback;
}

@Injectable({
  providedIn: 'root',
})
export class Api {
  private http = inject(HttpClient);

  /** Builds the Authorization header that carries the session token. */
  private static auth(token: string): { Authorization: string } {
    return { Authorization: `Bearer ${token}` };
  }

  lookup(word: string): Observable<HttpResponse<WordResponse | WordNotFound | WordError>> {
    return this.http.get<WordResponse | WordNotFound | WordError>(
      endpoint(`/api/word/${this.encodePath(word)}`),
      { observe: 'response' },
    );
  }

  suggest(word: string): Observable<string[]> {
    return this.http.get<string[]>(endpoint(`/api/suggest/${this.encodePath(word)}`));
  }

  synonym(word: string): Observable<string[]> {
    return this.http.get<string[]>(endpoint(`/api/synonym/${this.encodePath(word)}`));
  }

  /** Word of the day — deterministic per calendar day, same shape as a lookup hit. */
  wordOfTheDay(): Observable<HttpResponse<WordResponse | WordError>> {
    return this.http.get<WordResponse | WordError>(endpoint('/api/word-of-the-day'), {
      observe: 'response',
    });
  }

  autofill(
    word: string,
    searchHistory: string[],
    suggested: string[],
  ): Observable<AutofillResponse> {
    let params = new HttpParams();

    // The engine expects JSON array strings, e.g. ["word1","word2"].
    if (searchHistory.length > 0) params = params.set('history', JSON.stringify(searchHistory));
    if (suggested.length > 0) params = params.set('suggested', JSON.stringify(suggested));

    return this.http.get<AutofillResponse>(endpoint(`/api/autofill/${this.encodePath(word)}`), {
      params,
    });
  }

  // encodes word before its interpolated into the URL path, e.g. "hello world" -> "hello+world"
  private encodePath(value: string): string {
    return encodeURIComponent(value).replace(/%20/g, '+');
  }

  // AUTH_TIMEOUT_MS guards the auth calls below: without it, a slow or hung
  // backend leaves the login/signup page stuck on its disabled submit button.
  private static readonly AUTH_TIMEOUT_MS = 15000;

  signup(email: string, password: string, displayName: string): Observable<AuthResponse> {
    const body = new HttpParams()
      .set('email', email)
      .set('password', password)
      .set('displayName', displayName);
    return this.http
      .post<AuthResponse>(endpoint('/api/auth/signup'), body)
      .pipe(timeout(Api.AUTH_TIMEOUT_MS)); // add timeout to prevent hanging
  }

  login(email: string, password: string): Observable<AuthResponse> {
    const body = new HttpParams().set('email', email).set('password', password);
    return this.http
      .post<AuthResponse>(endpoint('/api/auth/login'), body)
      .pipe(timeout(Api.AUTH_TIMEOUT_MS));
  }

  logout(token: string): Observable<{ message: string }> {
    return this.http
      .post<{ message: string }>(endpoint('/api/auth/logout'), null, { headers: Api.auth(token) })
      .pipe(timeout(Api.AUTH_TIMEOUT_MS));
  }

  refresh(token: string): Observable<AuthResponse> {
    return this.http
      .post<AuthResponse>(endpoint('/api/auth/refresh'), null, { headers: Api.auth(token) })
      .pipe(timeout(Api.AUTH_TIMEOUT_MS));
  }

  changePassword(
    token: string,
    oldPassword: string,
    newPassword: string,
  ): Observable<{ message: string }> {
    const body = new HttpParams().set('oldPassword', oldPassword).set('newPassword', newPassword);
    return this.http
      .post<{ message: string }>(endpoint('/api/auth/change-password'), body, {
        headers: Api.auth(token),
      })
      .pipe(timeout(Api.AUTH_TIMEOUT_MS));
  }

  deleteAccount(token: string): Observable<{ message: string }> {
    return this.http
      .post<{ message: string }>(endpoint('/api/auth/delete-account'), null, {
        headers: Api.auth(token),
      })
      .pipe(timeout(Api.AUTH_TIMEOUT_MS));
  }

  me(token: string): Observable<AuthUser> {
    return this.http
      .get<AuthUser>(endpoint('/api/auth/me'), { headers: Api.auth(token) })
      .pipe(timeout(Api.AUTH_TIMEOUT_MS));
  }

  updateProfile(token: string, displayName: string, email: string): Observable<AuthUser> {
    const body = new HttpParams().set('displayName', displayName).set('email', email);
    return this.http
      .post<AuthUser>(endpoint('/api/auth/update'), body, { headers: Api.auth(token) })
      .pipe(timeout(Api.AUTH_TIMEOUT_MS));
  }

  listDocuments(token: string): Observable<DocumentSummary[]> {
    return this.http.get<DocumentSummary[]>(endpoint('/api/documents'), {
      headers: Api.auth(token),
    });
  }

  createDocument(token: string, title?: string): Observable<DocumentResponse> {
    let params = new HttpParams();
    if (title !== undefined) {
      params = params.set('title', title);
    }
    return this.http.post<DocumentResponse>(endpoint('/api/documents'), params, {
      headers: Api.auth(token),
    });
  }

  getDocument(token: string, id: number): Observable<DocumentResponse> {
    return this.http.get<DocumentResponse>(endpoint(`/api/documents/${id}`), {
      headers: Api.auth(token),
    });
  }

  saveDocument(token: string, id: number, content: string): Observable<DocumentResponse> {
    const body = new HttpParams().set('content', content);
    return this.http.put<DocumentResponse>(endpoint(`/api/documents/${id}`), body, {
      headers: Api.auth(token),
    });
  }

  renameDocument(token: string, id: number, title: string): Observable<DocumentResponse> {
    return this.http.post<DocumentResponse>(
      endpoint(`/api/documents/${id}/rename`),
      new HttpParams().set('title', title),
      { headers: Api.auth(token) },
    );
  }

  deleteDocument(token: string, id: number): Observable<{ message: string }> {
    return this.http.delete<{ message: string }>(endpoint(`/api/documents/${id}`), {
      headers: Api.auth(token),
    });
  }

  getSearchHistory(token: string): Observable<string[]> {
    return this.http.get<string[]>(endpoint('/api/search-history'), { headers: Api.auth(token) });
  }

  recordSearch(token: string, word: string): Observable<{ message: string }> {
    return this.http.post<{ message: string }>(
      endpoint('/api/search-history'),
      new HttpParams().set('word', word),
      { headers: Api.auth(token) },
    );
  }

  clearSearchHistory(token: string): Observable<{ message: string }> {
    return this.http.delete<{ message: string }>(endpoint('/api/search-history'), {
      headers: Api.auth(token),
    });
  }

  getSuggestedWords(token: string): Observable<string[]> {
    return this.http.get<string[]>(endpoint('/api/suggested-words'), { headers: Api.auth(token) });
  }

  /** Records many words at once; order is preserved (used to store synonyms). */
  syncSuggestedWords(token: string, words: string[]): Observable<{ message: string }> {
    let params = new HttpParams();
    for (const word of words) {
      params = params.append('word', word);
    }
    return this.http.post<{ message: string }>(endpoint('/api/suggested-words/sync'), params, {
      headers: Api.auth(token),
    });
  }

  clearSuggestedWords(token: string): Observable<{ message: string }> {
    return this.http.delete<{ message: string }>(endpoint('/api/suggested-words'), {
      headers: Api.auth(token),
    });
  }
}
