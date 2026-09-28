import { HttpResponse } from '@angular/common/http';
import { of } from 'rxjs';
import { Api } from '../services/api';
import { AUTOFILL_RESPONSE, HELLO_WORD, TEST_DOCUMENT, TEST_SESSION, TEST_USER } from './fixtures';

/**
 * Builds an in-memory Api whose methods return canned success values. Tests
 * override individual methods per-call:
 *
 *   const api = createApiStub();
 *   vi.spyOn(api, 'login').mockReturnValue(throwError(() => ({ status: 401, error: { error: 'x' } })));
 */
export function createApiStub(): Api {
  // Partial<Api> makes every Api method optional, so we can override only the ones we want in tests.
  const stub: Partial<Api> = {
    lookup: () => of(new HttpResponse({ body: HELLO_WORD, status: 200 })),
    wordOfTheDay: () => of(new HttpResponse({ body: HELLO_WORD, status: 200 })),
    suggest: () => of(['hello']),
    synonym: () => of(['hi', 'hey']),
    autofill: () => of(AUTOFILL_RESPONSE),
    login: () => of(TEST_SESSION),
    signup: () => of(TEST_SESSION),
    logout: () => of({ message: 'User logged out successfully.' }),
    refresh: () => of(TEST_SESSION),
    changePassword: () => of({ message: 'Password changed successfully.' }),
    deleteAccount: () => of({ message: 'Account deleted successfully.' }),
    me: () => of(TEST_USER),
    updateProfile: () => of(TEST_USER),
    listDocuments: () => of([TEST_DOCUMENT]),
    createDocument: () => of(TEST_DOCUMENT),
    getDocument: () => of(TEST_DOCUMENT),
    saveDocument: () => of(TEST_DOCUMENT),
    renameDocument: () => of(TEST_DOCUMENT),
    deleteDocument: () => of({ message: 'Document deleted.' }),
    getSearchHistory: () => of(['apple', 'banana']),
    recordSearch: () => of({ message: 'Search recorded.' }),
    clearSearchHistory: () => of({ message: 'Search history cleared.' }),
    getSuggestedWords: () => of(['syn1']),
    syncSuggestedWords: () => of({ message: 'Suggested words synced.' }),
    clearSuggestedWords: () => of({ message: 'Suggested words cleared.' }),
  };
  return stub as Api; // cast to Api
}

/** TestBed provider for components/services that depend on the Api. */
export function provideApiStub(stub: Api = createApiStub()) {
  return { provide: Api, useValue: stub };
}
