import { HttpRequest, provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { firstValueFrom, lastValueFrom } from 'rxjs';
import { Wotd } from '../wotd';
import { HELLO_WORD } from '../../testing/fixtures';

describe('Wotd', () => {
  let service: Wotd;
  let httpMock: HttpTestingController;

  const byPath = (url: string) => (req: HttpRequest<unknown>) => req.url === url;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting()],
    });
    service = TestBed.inject(Wotd);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => {
    httpMock.verify();
  });

  it('stores the returned word and settles ready with the word', async () => {
    const promise = lastValueFrom(service.fetch());

    const req = httpMock.expectOne(byPath('/api/word-of-the-day'));
    req.flush(HELLO_WORD, { status: 200, statusText: 'OK' });

    const word = await promise;
    expect(word).toEqual(HELLO_WORD);
    expect(service.word()).toEqual(HELLO_WORD);
    expect(service.ready()).toBe(true);
    expect(await firstValueFrom(service.settled)).toEqual(HELLO_WORD);
  });

  it('settles ready with no word on a server error', async () => {
    const promise = lastValueFrom(service.fetch());

    const req = httpMock.expectOne(byPath('/api/word-of-the-day'));
    req.flush({ error: 'No words available' }, { status: 500, statusText: 'Server Error' });

    const word = await promise;
    expect(word).toBeNull();
    expect(service.word()).toBeNull();
    expect(service.ready()).toBe(true);
    expect(await firstValueFrom(service.settled)).toBeNull();
  });

  it('settles ready with no word on a network error', async () => {
    const promise = lastValueFrom(service.fetch());

    const req = httpMock.expectOne(byPath('/api/word-of-the-day'));
    req.error(new ProgressEvent('Network error'));

    const word = await promise;
    expect(word).toBeNull();
    expect(service.word()).toBeNull();
    expect(service.ready()).toBe(true);
    expect(await firstValueFrom(service.settled)).toBeNull();
  });
});
