import { Component, OnInit, OnDestroy, HostListener, inject } from '@angular/core';
import { Router, RouterOutlet } from '@angular/router';
import { Subscription, timer } from 'rxjs';
import { Header } from './shared/header/header';
import { Footer } from './shared/footer/footer';
import { Auth } from './services/auth';
import { Wotd } from './services/wotd';

@Component({
  selector: 'app-root',
  imports: [RouterOutlet, Header, Footer],
  templateUrl: './app.html',
  styleUrl: './app.css',
})
export class App implements OnInit, OnDestroy {
  /**
   * Safety valve so a dead backend can never strand the user on the splash.
   * Sized above the real cold-start cost of the Render free tier (documented at
   * up to ~90s in render.yaml) so the splash normally outlives the wait and the
   * user lands on a page that already shows the day's word. A shorter cap would
   * drop them on an empty dictionary and then fill it in late, which is the
   * behaviour this is meant to avoid.
   */
  private static readonly SPLASH_MAX_MS = 60000;
  private static readonly SPLASH_ID = 'app-splash';

  constructor(private router: Router) {}

  private auth = inject(Auth);
  private wotd = inject(Wotd);
  private authSub?: Subscription;
  private wotdSub?: Subscription;
  private capSub?: Subscription;
  private splashHidden = false;

  ngOnInit() {
    // Keep the user's session alive across visits. This deliberately does NOT
    // gate the splash: refreshSession self-times-out at AUTH_TIMEOUT_MS, so on a
    // cold start it gives up long before the daily word arrives and would drop
    // the user on an empty page. The dictionary renders fine without it.
    this.authSub = this.auth.refreshSession().subscribe();

    // The splash is gated on the daily word alone, so the page underneath is
    // already showing it by the time the splash fades out. Wotd.fetch() settles
    // on failure too, so this fires whether the backend answers or not.
    this.wotdSub = this.wotd.fetch().subscribe({
      next: () => this.hideSplash(),
      error: () => this.hideSplash(),
    });

    this.capSub = timer(App.SPLASH_MAX_MS).subscribe(() => this.hideSplash());
  }

  ngOnDestroy() {
    this.authSub?.unsubscribe();
    this.wotdSub?.unsubscribe();
    this.capSub?.unsubscribe();
  }

  private hideSplash() {
    if (this.splashHidden) return;
    this.splashHidden = true;
    const el = document.getElementById(App.SPLASH_ID);
    if (!el) return;
    el.classList.add('hidden');
    setTimeout(() => el.remove(), 400); // match the CSS fade-out
  }

  @HostListener('document:keydown.escape')
  onEscape() {
    this.router.navigate(['/']);
  }
}
