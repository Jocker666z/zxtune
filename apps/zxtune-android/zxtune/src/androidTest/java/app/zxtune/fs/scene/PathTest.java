package app.zxtune.fs.scene;

import android.net.Uri;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import static org.junit.Assert.*;

import org.junit.Test;
import org.junit.runner.RunWith;

import app.zxtune.test.BuildConfig;

// dir/file.mp3
@RunWith(AndroidJUnit4.class)
public class PathTest {

  @Test
  public void testEmpty() {
    final Path path = Path.create();
    verifyEmpty(path);
    verifyDir(path.getChild("dir"));
  }

  @Test
  public void testDir() {
    final Path path = Path.parse(Uri.parse("scene:/dir"));
    verifyDir(path);
    verifyEmpty(path.getParent());
    verifyFile(path.getChild("file.mp3"));
  }

  @Test
  public void testFile() {
    final Path path = Path.parse(Uri.parse("scene:/dir/file.mp3"));
    verifyFile(path);
    verifyDir(path.getParent());
  }

  @Test
  public void testForeign() {
    final Path path = Path.parse(Uri.parse("foreign:/uri/test"));
    assertNull(path);
  }

  private static void verifyEmpty(Path path) {
    final Uri[] uris = path.getRemoteUris();
    assertEquals("getRemoteUris.length", 2, uris.length);
    assertEquals("getRemoteUris[0]", BuildConfig.CDN_ROOT + "/browse/scene/music/",
        uris[0].toString());
    assertEquals("getRemoteUris[1]", "https://archive.scene.org/pub/music/", uris[1].toString());
    assertEquals("getLocalId", "", path.getLocalId());
    assertEquals("getUri", "scene:", path.getUri().toString());
    assertEquals("getName", "",path.getName());
    assertNull("getParent", path.getParent());
    assertTrue("isEmpty", path.isEmpty());
    assertFalse("isFile", path.isFile());
  }

  private static void verifyDir(Path path) {
    final Uri[] uris = path.getRemoteUris();
    assertEquals("getRemoteUris.length", 2, uris.length);
    assertEquals("getRemoteUris[0]", BuildConfig.CDN_ROOT + "/browse/scene/music/dir/", uris[0].toString());
    assertEquals("getRemoteUris[1]", "https://archive.scene.org/pub/music/dir/", uris[1].toString());
    assertEquals("getLocalId", "dir", path.getLocalId());
    assertEquals("getUri", "scene:/dir", path.getUri().toString());
    assertEquals("getName", "dir", path.getName());
    assertFalse("isEmpty", path.isEmpty());
    assertFalse("isFile", path.isFile());
  }

  private static void verifyFile(Path path) {
    final Uri[] uris = path.getRemoteUris();
    assertEquals("getRemoteUris.length", 2, uris.length);
    assertEquals("getRemoteUris[0]", BuildConfig.CDN_ROOT + "/browse/scene/music/dir/file.mp3",
        uris[0].toString());
    assertEquals("getRemoteUris[1]", "https://archive.scene.org/pub/music/dir/file.mp3",
        uris[1].toString());
    assertEquals("getLocalId", "dir/file.mp3", path.getLocalId());
    assertEquals("getUri", "scene:/dir/file.mp3", path.getUri().toString());
    assertEquals("getName", "file.mp3", path.getName());
    assertFalse("isEmpty", path.isEmpty());
    assertTrue("isFile", path.isFile());
  }
}
