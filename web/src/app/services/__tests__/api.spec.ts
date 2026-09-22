import { HttpRequest, provideHttpClient } from '@angular/common/http';
import {
  HttpTestingController,
  TestRequest,
  provideHttpClientTesting,
} from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { firstValueFrom } from 'rxjs'; // converts an observable to a promise (subscribes, waits for the first emission, unsubscribes
// unsubscribes, and resolves the promise with that value)
import { Api, apiErrorMessage } from '../api';
import {
  AUTOFILL_RESPONSE,
  HELLO_WORD,
  TEST_DOCUMENT,
  TEST_SESSION,
  TEST_USER,
  WORD_ERROR,
  WORD_NOT_FOUND,
} from '../../testing/fixtures';

describe('Api', () => {
  let api: Api;
  let httpMock: HttpTestingController;

  /** Matches a request by path only, ignoring query params. */
  const byUrl = (url: string) => (req: HttpRequest<unknown>) => req.url === url;

  /** Asserts the request carries the session token as a Bearer Authorization header. */
  const expectAuth = (req: TestRequest, token = 't0ken') => {
    expect(req.request.headers.get('Authorization')).toBe(`Bearer ${token}`);
  };

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting()],
    });
    api = TestBed.inject(Api);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  /** The flow for each test: (isolate the frontend)
   * 1. Call the API method, which returns an observable (unresolved promise)
   * 2. Capture the request with `expectOne`, which returns a mock request
   * 3. Flush the mock request with a response, which resolves the promise
   * 4. Await the promise (unwraps the observable)
   */
  describe('word endpoints', () => {
    it('lookup GETs the raw word and decodes the response body', async () => {
      const promise = firstValueFrom(api.lookup('hello')); // unresolved

      const req = httpMock.expectOne('/api/word/hello'); // retrives captured request
      expect(req.request.method).toBe('GET');
      req.flush(HELLO_WORD, { status: 200, statusText: 'OK' }); // releases the captured request, emitting the value/error

      const response = await promise; // resolves with the flushed data
      expect(response.status).toBe(200);
      expect(response.body).toEqual(HELLO_WORD);
    });

    it('lookup surfaces a 404 miss as an error response with the body attached', async () => {
      const promise = firstValueFrom(api.lookup('zzzz'));

      const req = httpMock.expectOne('/api/word/zzzz');
      req.flush(WORD_NOT_FOUND, { status: 404, statusText: 'Not Found' });

      await expect(promise).rejects.toMatchObject({ status: 404, error: WORD_NOT_FOUND });
    });

    it('lookup encodes the word into the path', async () => {
      const promise = firstValueFrom(api.lookup('rock n roll'));

      const req = httpMock.expectOne('/api/word/rock+n+roll');
      req.flush(WORD_ERROR, { status: 400, statusText: 'Bad Request' });

      await expect(promise).rejects.toMatchObject({ status: 400, error: WORD_ERROR });
    });

    it('suggest GETs the suggestion endpoint', async () => {
      const promise = firstValueFrom(api.suggest('hell'));

      const req = httpMock.expectOne('/api/suggest/hell');
      expect(req.request.method).toBe('GET');
      req.flush(['hello']);

      expect(await promise).toEqual(['hello']);
    });

    it('synonym GETs the synonym endpoint', async () => {
      const promise = firstValueFrom(api.synonym('hello'));

      const req = httpMock.expectOne('/api/synonym/hello');
      req.flush(['hi', 'hey']);

      expect(await promise).toEqual(['hi', 'hey']);
    });

    it('autofill sends history and suggested words as JSON params', async () => {
      const promise = firstValueFrom(api.autofill('hello', ['help', 'world'], ['hi', 'hey']));

      const req = httpMock.expectOne((r) => r.url === '/api/autofill/hello');
      expect(req.request.params.get('history')).toBe(JSON.stringify(['help', 'world']));
      expect(req.request.params.get('suggested')).toBe(JSON.stringify(['hi', 'hey']));
      req.flush(AUTOFILL_RESPONSE);

      expect(await promise).toEqual(AUTOFILL_RESPONSE);
    });

    it('autofill omits history and suggested params when empty', async () => {
      const promise = firstValueFrom(api.autofill('hello', [], []));

      const req = httpMock.expectOne((r) => r.url === '/api/autofill/hello');
      expect(req.request.params.has('history')).toBe(false);
      expect(req.request.params.has('suggested')).toBe(false);
      req.flush(AUTOFILL_RESPONSE);

      await promise;
    });

    it('wordOfTheDay GETs the endpoint and decodes the response body', async () => {
      const promise = firstValueFrom(api.wordOfTheDay());

      const req = httpMock.expectOne('/api/word-of-the-day');
      expect(req.request.method).toBe('GET');
      req.flush(HELLO_WORD, { status: 200, statusText: 'OK' });

      const response = await promise;
      expect(response.status).toBe(200);
      expect(response.body).toEqual(HELLO_WORD);
    });

    it('wordOfTheDay surfaces a 500 server error with the body attached', async () => {
      const promise = firstValueFrom(api.wordOfTheDay());

      const req = httpMock.expectOne('/api/word-of-the-day');
      req.flush({ error: 'No words available' }, { status: 500, statusText: 'Server Error' });

      await expect(promise).rejects.toMatchObject({
        status: 500,
        error: { error: 'No words available' },
      });
    });
  });

  describe('auth endpoints', () => {
    it('signup posts the email, password, and display name', async () => {
      const promise = firstValueFrom(api.signup('me@test.com', 'raven1', 'Me'));

      const req = httpMock.expectOne('/api/auth/signup');
      expect(req.request.method).toBe('POST');
      expect(req.request.body.get('email')).toBe('me@test.com');
      expect(req.request.body.get('password')).toBe('raven1');
      expect(req.request.body.get('displayName')).toBe('Me');
      req.flush(TEST_SESSION);

      expect(await promise).toEqual(TEST_SESSION);
    });

    it('login posts the email and password', async () => {
      const promise = firstValueFrom(api.login('me@test.com', 'raven1'));

      const req = httpMock.expectOne('/api/auth/login');
      expect(req.request.method).toBe('POST');
      expect(req.request.body.get('email')).toBe('me@test.com');
      expect(req.request.body.get('password')).toBe('raven1');
      req.flush(TEST_SESSION);

      expect(await promise).toEqual(TEST_SESSION);
    });

    it('logout sends the session token as a bearer header', async () => {
      const promise = firstValueFrom(api.logout('t0ken'));

      const req = httpMock.expectOne('/api/auth/logout');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body).toBeNull();
      req.flush({ message: 'User logged out successfully.' });

      expect((await promise).message).toBe('User logged out successfully.');
    });

    it('refresh sends the token as a bearer header and returns a new session', async () => {
      const promise = firstValueFrom(api.refresh('t0ken'));

      const req = httpMock.expectOne('/api/auth/refresh');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body).toBeNull();
      req.flush(TEST_SESSION);

      expect(await promise).toEqual(TEST_SESSION);
    });

    it('me GETs the profile with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.me('t0ken'));

      const req = httpMock.expectOne(byUrl('/api/auth/me'));
      expect(req.request.method).toBe('GET');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush(TEST_USER);

      expect(await promise).toEqual(TEST_USER);
    });

    it('updateProfile posts the token as a bearer header, display name, and email', async () => {
      const promise = firstValueFrom(api.updateProfile('t0ken', 'New Name', 'new@test.com'));

      const req = httpMock.expectOne('/api/auth/update');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body.get('displayName')).toBe('New Name');
      expect(req.request.body.get('email')).toBe('new@test.com');
      expect(req.request.body.get('token')).toBeNull();
      req.flush(TEST_USER);

      expect(await promise).toEqual(TEST_USER);
    });

    it('changePassword sends the token as a bearer header plus old and new password', async () => {
      const promise = firstValueFrom(api.changePassword('t0ken', 'oldpassword', 'newpassword'));

      const req = httpMock.expectOne('/api/auth/change-password');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body.get('oldPassword')).toBe('oldpassword');
      expect(req.request.body.get('newPassword')).toBe('newpassword');
      expect(req.request.body.get('token')).toBeNull();
      req.flush({ message: 'Password changed successfully.' });

      expect((await promise).message).toBe('Password changed successfully.');
    });

    it('deleteAccount sends the token as a bearer header', async () => {
      const promise = firstValueFrom(api.deleteAccount('t0ken'));

      const req = httpMock.expectOne('/api/auth/delete-account');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body).toBeNull();
      req.flush({ message: 'Account deleted successfully.' });

      expect((await promise).message).toBe('Account deleted successfully.');
    });
  });

  describe('document endpoints', () => {
    it('listDocuments GETs with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.listDocuments('t0ken'));

      const req = httpMock.expectOne(byUrl('/api/documents'));
      expect(req.request.method).toBe('GET');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush([TEST_DOCUMENT]);

      expect(await promise).toEqual([TEST_DOCUMENT]);
    });

    it('createDocument POSTs the title when given, with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.createDocument('t0ken', 'My Story'));

      const req = httpMock.expectOne('/api/documents');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body.get('title')).toBe('My Story');
      expect(req.request.body.get('token')).toBeNull();
      req.flush(TEST_DOCUMENT);

      expect(await promise).toEqual(TEST_DOCUMENT);
    });

    it('createDocument omits the title param when none is given', async () => {
      const promise = firstValueFrom(api.createDocument('t0ken'));

      const req = httpMock.expectOne('/api/documents');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.params.get('title')).toBeNull();
      req.flush(TEST_DOCUMENT);

      expect(await promise).toEqual(TEST_DOCUMENT);
    });

    it('getDocument GETs by id with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.getDocument('t0ken', 1));

      const req = httpMock.expectOne(byUrl('/api/documents/1'));
      expect(req.request.method).toBe('GET');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush(TEST_DOCUMENT);

      expect(await promise).toEqual(TEST_DOCUMENT);
    });

    it('saveDocument PUTs the content in the body, with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.saveDocument('t0ken', 1, 'my note'));

      const req = httpMock.expectOne('/api/documents/1');
      expect(req.request.method).toBe('PUT');
      expectAuth(req);
      expect(req.request.body.get('content')).toBe('my note');
      expect(req.request.body.get('token')).toBeNull();
      req.flush(TEST_DOCUMENT);

      expect(await promise).toEqual(TEST_DOCUMENT);
    });

    it('renameDocument POSTs the new title in the body', async () => {
      const promise = firstValueFrom(api.renameDocument('t0ken', 1, 'Renamed'));

      const req = httpMock.expectOne('/api/documents/1/rename');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body.get('title')).toBe('Renamed');
      expect(req.request.body.get('token')).toBeNull();
      req.flush(TEST_DOCUMENT);

      expect(await promise).toEqual(TEST_DOCUMENT);
    });

    it('deleteDocument DELETEs by id with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.deleteDocument('t0ken', 1));

      const req = httpMock.expectOne(byUrl('/api/documents/1'));
      expect(req.request.method).toBe('DELETE');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush({ message: 'Document deleted.' });

      expect((await promise).message).toBe('Document deleted.');
    });
  });

  describe('per-user endpoints', () => {
    it('getSearchHistory GETs with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.getSearchHistory('t0ken'));

      const req = httpMock.expectOne(byUrl('/api/search-history'));
      expect(req.request.method).toBe('GET');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush(['apple', 'banana']);

      expect(await promise).toEqual(['apple', 'banana']);
    });

    it('recordSearch POSTs the word in the body, with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.recordSearch('t0ken', 'apple'));

      const req = httpMock.expectOne('/api/search-history');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body.get('word')).toBe('apple');
      expect(req.request.body.get('token')).toBeNull();
      req.flush({ message: 'Search recorded.' });

      expect((await promise).message).toBe('Search recorded.');
    });

    it('clearSearchHistory DELETEs with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.clearSearchHistory('t0ken'));

      const req = httpMock.expectOne(byUrl('/api/search-history'));
      expect(req.request.method).toBe('DELETE');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush({ message: 'Search history cleared.' });

      expect((await promise).message).toBe('Search history cleared.');
    });

    it('getSuggestedWords GETs with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.getSuggestedWords('t0ken'));

      const req = httpMock.expectOne(byUrl('/api/suggested-words'));
      expect(req.request.method).toBe('GET');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush(['syn1']);

      expect(await promise).toEqual(['syn1']);
    });

    it('syncSuggestedWords POSTs each word as a body param', async () => {
      const promise = firstValueFrom(api.syncSuggestedWords('t0ken', ['hi', 'hey']));

      const req = httpMock.expectOne('/api/suggested-words/sync');
      expect(req.request.method).toBe('POST');
      expectAuth(req);
      expect(req.request.body.getAll('word')).toEqual(['hi', 'hey']);
      expect(req.request.body.get('token')).toBeNull();
      req.flush({ message: 'Suggested words synced.' });

      expect((await promise).message).toBe('Suggested words synced.');
    });

    it('clearSuggestedWords DELETEs with the token as a bearer header', async () => {
      const promise = firstValueFrom(api.clearSuggestedWords('t0ken'));

      const req = httpMock.expectOne(byUrl('/api/suggested-words'));
      expect(req.request.method).toBe('DELETE');
      expectAuth(req);
      expect(req.request.params.get('token')).toBeNull();
      req.flush({ message: 'Suggested words cleared.' });

      expect((await promise).message).toBe('Suggested words cleared.');
    });
  });

  describe('apiErrorMessage', () => {
    it('prefers the server-provided message', () => {
      expect(apiErrorMessage({ error: { error: 'Wrong password.' } }, 'fallback')).toBe(
        'Wrong password.',
      );
    });

    it('maps status codes to friendly messages', () => {
      expect(apiErrorMessage({ status: 404 }, 'fallback')).toBe(
        'This feature is not available right now. Please try again later.',
      );
      expect(apiErrorMessage({ status: 409 }, 'fallback')).toBe(
        'An account with this email already exists.',
      );
      expect(apiErrorMessage({ status: 401 }, 'fallback')).toBe('Invalid email or password.');
    });

    it('falls back when nothing matches', () => {
      expect(apiErrorMessage({ status: 500 }, 'fallback')).toBe('fallback');
      expect(apiErrorMessage({}, 'fallback')).toBe('fallback');
    });
  });
});
